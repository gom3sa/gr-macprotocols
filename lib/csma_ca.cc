/*
This CSMA/CA is based on IEEE Std 802.11-2016. Consult "IEEE Standard for Information Techonology - Local
and Metropolitan Area Networks - Specific Requirements, part 11: Wireless LAN MAC and PHY Specifications"
for more details. Although this is based on the mentioned document, you should keep in mind this is a 
CSMA/CA flavour implementation. IEEE 802.11 implements DCF, which has slightly significant differences.

Author: André Gomes, andre.gomes@dcc.ufmg.br - Winet Lab, Federal University of Minas Gerais, Brazil.
*/

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
#include <chrono>

#define MAX_TRIES 5
#define RxPHYDelay 1 // (us) for max distance of 300m between nodes
#define THRESHOLD -70 // Empirical
#define AVG_BLOCK_DELAY 10 // (us)
#define aCWmin 16 // aCWmin + 1
#define aCWmax 1024 // aCWmax + 1

/* aCWm** from www.revolutionwifi.net/revolutionwifi/2010/08/wireless-qos-part-5-contention-window.html
    802.11b    aCWmin 31    aCWmax 1023
    802.11g    aCWmin 31    aCWmax 1023 (when 802.11b stations are present)
    802.11g    aCWmin 15    aCWmax 1023
    802.11a    aCWmin 15    aCWmax 1023
    802.11n    aCWmin 15    aCWmax 1023
*/

using namespace gr::macprotocols;

class csma_ca_impl : public csma_ca {

	typedef std::chrono::high_resolution_clock clock;

	public:
		csma_ca_impl(std::vector<uint8_t> src_mac, int slot_time, int sifs, int difs, int alpha, bool debug) : gr::block(
							"csma_ca",
							gr::io_signature::make(0, 0, 0),
							gr::io_signature::make(0, 0, 0)),
							pr_slot_time(slot_time*alpha), pr_sifs(sifs*alpha), pr_difs(difs*alpha), pr_alpha(alpha), pr_debug(debug), pr_frame_id(0) {
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
			message_port_register_out(msg_port_frame_to_app);

			// Variables initialization
			pr_status = false; // FALSE: iddle, no frame to send; TRUE: busy, trying to send a frame. 
			pr_frame_acked = false; // TRUE: ack was just received. This is usefull for thread handling send_frame().
			pr_cw = aCWmin;


			for(int i = 0; i < 6; i++) pr_mac_addr[i] = src_mac[i];
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
				if(!pr_status) { // This means no frame has arrived to be sent. So, it will request one to buffer.
					message_port_pub(msg_port_frame_request, pmt::string_to_symbol("get frame"));
				}
				usleep((rand() % 5)*(pr_slot_time + pr_sifs + pr_difs) + AVG_BLOCK_DELAY*pr_alpha); srand(time(NULL));
			}
		}

		void frame_from_buff(pmt::pmt_t frame) {
			boost::unique_lock<boost::mutex> lock(mu1);
			if(pr_debug) std::cout << "New frame from app" << std::endl << std::flush;

			if(!pr_status) {
				pr_status = true; // This means that there is already one frame to be sent. No more frames will be requested meanwhile.
				pr_frame_acked = false;
				pr_frame = frame; // Frame to be sent.
				thread_send_frame = boost::shared_ptr<gr::thread::thread> (new gr::thread::thread(boost::bind(&csma_ca_impl::send_frame, this)));
			}

			lock.unlock();
		}

		void send_frame() {
			if(pr_debug) std::cout << "Sending frame..." << std::endl << std::flush;

			uint attempts = 0; // Counter for retransmission.
			uint sensing_time = pr_difs; 
			uint backoff = 0;

			uint tot_attempts = 0; // This counter takes into account scenarios where the medium is busy. So, discard frame after a while.

			while(attempts < MAX_TRIES and pr_frame_acked == false and tot_attempts < MAX_TRIES) {

				// This call listens to the medium for "sensing_time" us. This is used for both DIFS and AIFS Backoff.
				bool ch_busy = is_channel_busy(THRESHOLD, sensing_time);
				
				if(pr_debug) std::cout << "Is channel busy? " << ch_busy << ", frame acked? " << pr_frame_acked << std::endl << std::flush;
				
				if(ch_busy == false  and pr_frame_acked == false) { // Transmit
					message_port_pub(msg_port_frame_to_phy, pr_frame);
					attempts++;

					// Waiting for ack. Counting down for timeout.
					int timeout = pr_sifs + pr_slot_time + RxPHYDelay*pr_alpha;
					if(pr_debug) std::cout << "Waiting for ack. Timeout = " << timeout << std::endl << std::flush;
					auto start_time = clock::now();
					auto end_time = clock::now();
					float duration = (float) std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
					while(duration < timeout) {
						end_time = clock::now();
						duration = (float) std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
					}

				} else if(ch_busy == true and pr_frame_acked == false) { // Randomize backoff, increment contention window (pr_cw).
					// BackOffTime = Random() x aSlotTime, Random = [0, cw] where aCWmin <= cw <= aCWmax.
					backoff = rand() % pr_cw;
					pr_cw = pr_cw*2;
					if(pr_cw >= aCWmax) pr_cw = aCWmax;

					sensing_time = backoff*pr_slot_time;

					if(pr_debug) std::cout << "Randomize backoff. Current value = " << sensing_time << " (us)." << std::endl << std::flush;

				} /*
					IF YOU UNCOMMENT this part, make sure to replace "sensing_time = backoff*pr_slot_time" to "sensing_time = pr_slot_time" on line above.
						ALSO, add "and backoff <= 0" on the first if (skip debug one). REASON for letting it the way it is: checking medium
						by a slottime at time maybe inefficient while doing carrier sensing due to timing constraints. Really hard to keep 
						precision in such a small time window.

					else if(ch_busy == false and pr_frame_acked == false and backoff > 0) { // Decrement backoff
					backoff--;
					if(backoff <= 0) {
						backoff = 0;
						sensing_time = 0;
					} else {
						sensing_time = pr_slot_time;
					}

					if(pr_debug) std::cout << "Decrement backoff. Current value = " << sensing_time*backoff << " (us)." << std::endl;
				}*/

				tot_attempts++;
			}

			if(pr_frame_acked) {
				pr_cw = aCWmin; // Sucessful transmission. So, reset contention window.
				if(pr_debug) std::cout << "Frame acked properly!" << std::endl << std::flush;
			} else if(attempts >= MAX_TRIES) {
				if(pr_debug) std::cout << "Max number of retries exceeded. Frame dropped!" << std::endl << std::flush;
			} else if(tot_attempts == MAX_TRIES) {
				if(pr_debug) std::cout << "Medium is too busy. Frame dropped!" << std::endl << std::flush;
			}

			pr_status = false;
		}

		bool is_channel_busy(int threshold, int time) {
			if(pr_debug) std::cout << "Request carrier sensing for " << time << " (us)." << std::endl;

			message_port_pub(msg_port_request_to_cs, pmt::string_to_symbol(std::to_string((float)time)));

			pr_sensing = true; 
			boost::unique_lock<boost::mutex> lock(mu2);
			while(pr_sensing) pr_cond1.wait(lock);

			if(pr_debug) std::cout << "Avg power from medium = " << pr_avg_power << " (dB)." << std::endl;

			if(pr_avg_power < threshold) {
				return false;
			} else {
				return true;
			}
		}

		void frame_from_phy(pmt::pmt_t frame) {
			// TODO: Check other types of frames.

			uint8_t tof = type_of_frame(frame);

			if(tof == 0) {
				if(pr_debug) std::cout << "This frame is not for me. Drop it!" << std::endl << std::flush;
			} 
			else if(tof == 1) {				
				frame = pmt::cdr(frame);
				mac_header *h1 = (mac_header*)pmt::blob_data(frame);

				frame = pmt::cdr(pr_frame);
				mac_header *h2 = (mac_header*)pmt::blob_data(frame);

				if(h1->seq_nr == h2->seq_nr) {
					pr_frame_acked = true; pr_status = false;
					if(pr_debug) std::cout << "Ack for me!" << std::endl << std::flush;
				}
				else {
					if(pr_debug) std::cout << "Old ack! Forget it..." << std::endl << std::flush;
				}
				
			}
			else if(tof == 2) {
				if(pr_debug) std::cout << "Data frame belongs to me. Ack sent!" << std::endl << std::flush;
				pmt::pmt_t ack = generate_ack_frame(frame);
				message_port_pub(msg_port_frame_to_phy, ack);
				message_port_pub(msg_port_frame_to_app, frame);
			}
			else {
				if(pr_debug) std::cout << "Unkown frame type." << std::endl << std::flush;
			}
		}

		void cs_in(pmt::pmt_t msg) {
			pr_avg_power = pmt::to_float(msg);
			
			pr_sensing = false;
			pr_cond1.notify_all();
		}

		int8_t type_of_frame(pmt::pmt_t frame) {
			/* Output meaning
			 * 0: Not for me
			 * 1: Ack for me
			 * 2: Data frame for me
			 * 3: Unkown frame type
			*/
			frame = pmt::cdr(frame);
			mac_header *h = (mac_header*)pmt::blob_data(frame);

			for(int i = 0; i < 6; i++) // Destination mac is not my mac addr
				if(h->addr1[i] != pr_mac_addr[i]) return 0;

			switch(h->frame_control) {
				case 0x2B00: return 1;
				case 0x0008: return 2;
				default: return 3;
			}
		}

		pmt::pmt_t generate_ack_frame(pmt::pmt_t frame) {
			pmt::pmt_t car_frame = pmt::car(frame);
			pmt::pmt_t cdr_frame = pmt::cdr(frame);

			//size_t frame_len = pmt::blob_length(cdr_frame); // Length in bytes
			size_t frame_len = 24; // Length in bytes. Copy only the MAC header, first 24 bytes

			mac_header *h = (mac_header*)pmt::blob_data(cdr_frame);
			h->frame_control = 0x2B00; // Stands for ACK on the Frame Control

			/* Update mac addresses */
			for(int i = 0; i < 6; i++) {
				h->addr1[i] = h->addr2[i];
				h->addr2[i] = pr_mac_addr[i];
			}

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
		int pr_slot_time, pr_sifs, pr_difs, pr_frame_id, pr_alpha;
		uint pr_cw;
		bool pr_debug, pr_sensing, pr_status, pr_frame_acked;
		float pr_avg_power;
		boost::condition_variable pr_cond1;
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
		pmt::pmt_t msg_port_frame_to_app = pmt::mp("frame to app");

		// MAC addr 
		uint8_t pr_mac_addr[6];

		// Frame to be sent
		pmt::pmt_t pr_frame;

};

csma_ca::sptr
csma_ca::make(std::vector<uint8_t> src_mac, int slot_time, int sifs, int difs, int alpha, bool debug) {
	return gnuradio::get_initial_sptr(new csma_ca_impl(src_mac, slot_time, sifs, difs, alpha, debug));
}
