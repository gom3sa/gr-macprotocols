#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include <gnuradio/block_detail.h>
#include "csma_ca.h"
#include <boost/thread.hpp>
#include <unistd.h>
#include <string>
#include <cstdlib>
#include <stdlib.h>
#include <time.h>
#include <boost/crc.hpp>

#define MAX_TRIES 5
#define RxPHYDelay 1 // (us) for max distance of 300m between nodes
#define THRESHOLD -80 // Empirical
#define AVG_BLOCK_DELAY 100 // (us)
#define aCWmin 15
#define aCWmax 1023

/* aCWm** from www.revolutionwifi.net/revolutionwifi/2010/08/wireless-qos-part-5-contention-window.html
    802.11b    aCWmin 31    aCWmax 1023
    802.11g    aCWmin 31    aCWmax 1023 (when 802.11b stations are present)
    802.11g    aCWmin 15    aCWmax 1023
    802.11a    aCWmin 15    aCWmax 1023
    802.11n    aCWmin 15    aCWmax 1023
*/

using namespace gr::macprotocols;

class csma_ca_impl : public csma_ca {

	public:
		csma_ca_impl(std::vector<uint8_t> src_mac, int slot_time, int sifs, int difs, int alpha, bool debug) : gr::block(
							"csma_ca",
							gr::io_signature::make(0, 0, 0),
							gr::io_signature::make(0, 0, 0)),
							slot_time(slot_time*alpha), sifs(sifs*alpha), difs(difs*alpha), alpha(alpha), debug(debug), frame_id(0) {
			// Inputs
			message_port_register_in(msg_port_frame_from_buff);
			set_msg_handler(msg_port_frame_from_buff, boost::bind(&csma_ca_impl::frame_from_buff, this, _1));

			message_port_register_in(msg_port_frame_from_phy);
			set_msg_handler(msg_port_frame_from_phy, boost::bind(&csma_ca_impl::frame_from_phy, this, _1));

			message_port_register_in(msg_port_cs_in);
			set_msg_handler(msg_port_cs_in, boost::bind(&csma_ca_impl::cs_in, this, _1));

			// Outputs
			message_port_register_out(msg_port_frame_to_phy);
			message_port_register_out(msg_port_frame_request);
			message_port_register_out(msg_port_request_to_cs);

			// Variables initialization
			status = false; // FALSE: iddle, no frame to send; TRUE: busy, trying to send a frame. 
			frame_acked = false; // TRUE: ack was just received. This is usefull for thread handling send_frame().

			for(int i = 0; i < 6; i++) mac_addr[i] = src_mac[i];
		}

		bool start() {
			/*
			 * The use of start() prevents the thread bellow to access the message port in check_buff() before it even exists. 
			 * This ensures the scheduler first deals with the msg port, then the thread is created.
			*/
			thread_check_buff = boost::shared_ptr<gr::thread::thread> (new gr::thread::thread(boost::bind(&csma_ca_impl::check_buff, this)));
			return block::start();
		}

		void check_buff() {
			// TODO: Find a more efficient way to get data from buffer only if block is iddle and buffer not empty
			while(true) {
				if(!status) { // This means no frame has arrived to be sent. So, it will request one to buffer.
					message_port_pub(msg_port_frame_request, pmt::string_to_symbol("get frame"));
				}
				usleep((rand() % 5)*(slot_time + sifs + difs) + AVG_BLOCK_DELAY); srand(time(NULL));
			}
		}

		void frame_from_buff(pmt::pmt_t frame) {
			boost::unique_lock<boost::mutex> lock(mu1);
			if(debug) std::cout << "New frame from app" << std::endl << std::flush;

			if(!status) {
				status = true; // This means that there is already one frame to be sent. No more frames will be requested meanwhile.
				frame_acked = false;
				thread_send_frame = boost::shared_ptr<gr::thread::thread> (new gr::thread::thread(boost::bind(&csma_ca_impl::send_frame, this, frame)));
			}

			lock.unlock();
		}

		void send_frame(pmt::pmt_t frame) {
			if(debug) std::cout << "Sending frame..." << std::endl << std::flush;

			uint attempts = 0;
			uint sensing_time = difs; 
			uint cw = aCWmin*alpha;

			while(attempts < MAX_TRIES and frame_acked == false) {
				if(!is_channel_busy(THRESHOLD, sensing_time) and frame_acked == false) {
					message_port_pub(msg_port_frame_to_phy, frame);
					attempts++;
				}
			}

			if(frame_acked)
				if(debug) std::cout << "Frame acked properly!" << std::endl << std::flush;
			else if(attempts >= MAX_TRIES)
				if(debug) std::cout << "Max number of retries exceeded. Frame dropped!" << std::endl << std::flush;

			// TODO: 1) implementation of backoff, 2) implementation of ack arrival function, 3) implementation of send_ack()
			status = false;
		}

		bool is_channel_busy(int threshold, int time) {
			if(debug) std::cout << "Sending sensing request to carrier sense" << std::endl << std::flush;
			message_port_pub(msg_port_request_to_cs, pmt::string_to_symbol("threshold=" + std::to_string(threshold) + ",time=" + std::to_string(time)));

			sensing = true; channel_busy = false;
			boost::unique_lock<boost::mutex> lock(mu2);
			while(sensing) cond1.wait(lock);

			return channel_busy;
		}

		void frame_from_phy(pmt::pmt_t frame) {
			// TODO: Check if it is: 1) an ack, 2) a frame
			// If an ack
			if(is_ack(frame)) {
				frame_acked = true; status = false;
			} 
			else {
				pmt::pmt_t ack_frame = generate_ack_frame(frame);
				message_port_pub(msg_port_frame_to_phy, ack_frame);
				if(debug) std::cout << "Frame received. ACK was sent!" << std::endl << std::flush;
			}
		}

		void cs_in(pmt::pmt_t msg) {
			std::string str = pmt::symbol_to_string(msg);
			if(str == "busy")
				channel_busy = true;
			else
				channel_busy = false;
			sensing = false;
			cond1.notify_all();
		}

		bool is_ack(pmt::pmt_t frame) {
			frame = pmt::cdr(frame);
			mac_header *h = (mac_header*)pmt::blob_data(frame);
			
			if(h->frame_control == 0x2B00) { // It's an ACK
				for(int i = 0; i < 6; i++) {
					if(debug) std::cout << "Dsr addr[" << i << "] = " << h->addr1[i] << ", my addr[" << i << "] = " << mac_addr[i] << std::endl << std::flush;
					if(h->addr1[i] != mac_addr[i]) {
						if(debug) std::cout << "An ACK was received but it is NOT mine." << std::endl << std::flush;
						return false; // ACK is not mine
					}
				}
				if(debug) std::cout << "ACK received!" << std::endl << std::flush;
				return true;
			}

			return false;
		}

		pmt::pmt_t generate_ack_frame(pmt::pmt_t frame) {
			pmt::pmt_t car_frame = pmt::car(frame);
			pmt::pmt_t cdr_frame = pmt::cdr(frame);

			size_t frame_len = pmt::blob_length(cdr_frame);

			mac_header *h = (mac_header*)pmt::blob_data(cdr_frame);
			h->frame_control = 0x2B00; // Stands for ACK on the Frame Control

			/* Calculate new checksum */
			boost::crc_32_type result;
			result.process_bytes(h, frame_len);
			uint32_t fcs = result.checksum();

			uint8_t psdu[1528];
			memcpy(psdu, h, frame_len);
			memcpy(psdu + frame_len, &fcs, sizeof(uint32_t));

			cdr_frame = pmt::make_blob(psdu, frame_len + sizeof(uint32_t));
			/* Done! Checksum updated. */

			return pmt::cons(car_frame, cdr_frame);
		}

	private:
		int slot_time, sifs, difs, frame_id, alpha;
		bool debug, sensing, channel_busy, status, frame_acked;
		boost::condition_variable cond1;
		boost::mutex mu1, mu2;
		boost::shared_ptr<gr::thread::thread> thread_check_buff, thread_send_frame;

		// Input ports
		pmt::pmt_t msg_port_frame_from_buff = pmt::mp("frame from buffer");
		pmt::pmt_t msg_port_frame_from_phy = pmt::mp("frame from phy");
		pmt::pmt_t msg_port_cs_in = pmt::mp("cs in");

		// Output ports
		pmt::pmt_t msg_port_frame_to_phy = pmt::mp("frame to phy");
		pmt::pmt_t msg_port_frame_request = pmt::mp("frame request");
		pmt::pmt_t msg_port_request_to_cs = pmt::mp("request to cs");

		// MAC addr 
		uint8_t mac_addr[6];

};

csma_ca::sptr
csma_ca::make(std::vector<uint8_t> src_mac, int slot_time, int sifs, int difs, int alpha, bool debug) {
	return gnuradio::get_initial_sptr(new csma_ca_impl(src_mac, slot_time, sifs, difs, alpha, debug));
}
