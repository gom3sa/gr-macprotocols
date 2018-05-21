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
#include <boost/circular_buffer.hpp>

#define MAX_LOCAL_BUFF 3
#define AVG_BLOCK_DELAY 1000 // us, so 1ms
#define MAX_RETRIES 10
#define RxPHYDelay 1 // (us) for max distance of 300m between nodes
#define aCWmin 16 // aCWmin + 1
#define aCWmax 1024 // aCWmax + 1
// Frame Control (FC) cheat sheet
#define FC_ACK 0x2B00
#define FC_DATA 0x0008
#define FC_PROTOCOL 0x2900 // Active protocol on network
#define FC_METRICS 0x2100

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
		csma_ca_impl(std::vector<uint8_t> src_mac, int slot_time, int sifs, int difs, int alpha, int threshold, bool debug) : gr::block(
							"csma_ca",
							gr::io_signature::make(0, 0, 0),
							gr::io_signature::make(0, 0, 0)),
							pr_slot_time(slot_time*alpha), pr_sifs(sifs*alpha), pr_difs(difs*alpha), pr_alpha(alpha), pr_threshold(threshold), pr_debug(debug), pr_frame_id(0) {
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
			pr_acked = false; // TRUE: ack was just received. This is usefull for thread handling send_frame().
			pr_cw = aCWmin;


			for(int i = 0; i < 6; i++) {
				pr_mac_addr[i] = src_mac[i];
				pr_broadcast_addr[i] = 0xff;
			}

			// Set local buffer capacity, should be the smallest possible
			pr_buff.rset_capacity(MAX_LOCAL_BUFF);
		}

		bool start() {
			/*
			 * The use of start() prevents the thread bellow to access the message port in check_buff() before it even exists. 
			 * This ensures the scheduler first deals with the msg port, then the thread is created.
			*/
			thread_send_frame = boost::shared_ptr<gr::thread::thread> (new gr::thread::thread(boost::bind(&csma_ca_impl::send_frame, this)));
			thread_check_buff = boost::shared_ptr<gr::thread::thread> (new gr::thread::thread(boost::bind(&csma_ca_impl::check_buff, this)));
			return block::start();
		}

		void check_buff() {
			// TODO: Find a more efficient way to get data from buffer only if block is iddle and buffer not empty
			while(true) {
				if(pr_buff.size() < MAX_LOCAL_BUFF) {
					message_port_pub(msg_port_frame_request, pmt::string_to_symbol("get frame"));
					usleep(3 * AVG_BLOCK_DELAY);
				} else {
					usleep((pr_slot_time + pr_sifs + pr_difs) * (MAX_LOCAL_BUFF*0.8));
				}
			}
		}

		void frame_from_buff(pmt::pmt_t frame) {
			if(pr_buff.size() < MAX_LOCAL_BUFF) {
				pr_buff.push_back(frame);
				pr_new_frame_cond.notify_all(); // In case send_frame() is waiting for a new frame
			} else {
				if(pr_debug) std::cout << "Local buffer is already FULL!" << std::endl << std::flush;
			}
		}

		void send_frame() {
			uint attempts, sensing_time, backoff, tot_attempts;
			bool ch_busy, is_broadcast;
			int timeout;
			decltype(clock::now()) tic, toc;
			float dt; 
			boost::unique_lock<boost::mutex> lock0(pr_mu0);

			if(pr_debug) std::cout << "Sending frame..." << std::endl << std::flush;

			while(true) {
				// Waiting for a new frame
				while(pr_buff.size() <= 0) pr_new_frame_cond.wait(lock0); 

				// In order to get sequence number
				pmt::pmt_t cdr = pmt::cdr(pr_buff[0]);
				mac_header *h = (mac_header*)pmt::blob_data(cdr);
				pr_frame_seq_nr = h->seq_nr;

				is_broadcast = false;
				if(memcmp(h->addr1, pr_broadcast_addr, 6) == 0) is_broadcast = true;

				attempts = 0; // Counter for retransmission.
				sensing_time = pr_difs; 
				backoff = 0;

				tot_attempts = 0; // This counter takes into account scenarios where the medium is busy. So, discard frame after a while.

				while(attempts < MAX_RETRIES and pr_acked == false and tot_attempts < MAX_RETRIES) {

					// This call listens to the medium for "sensing_time" us. This is used for both DIFS and AIFS Backoff.
					ch_busy = is_channel_busy(pr_threshold, sensing_time);
					
					if(pr_debug) std::cout << "Is channel busy? " << ch_busy << ", frame acked? " << pr_acked << std::endl << std::flush;
					
					if(ch_busy == false  and pr_acked == false) { // Transmit
						message_port_pub(msg_port_frame_to_phy, pr_buff[0]);
						attempts++;

						if(is_broadcast) { // No ACK is expected from broadcast frames
							pr_acked = true;
							if(pr_debug) std::cout << "Broadcast frame was sent!" << std::endl << std::flush;
						}

						// Waiting for ack. Counting down for timeout.
						timeout = pr_sifs + pr_slot_time + RxPHYDelay*pr_alpha;
						if(pr_debug) std::cout << "Waiting for ack. Timeout = " << timeout << std::endl << std::flush;
						usleep(timeout);
						/* Busy waiting should not be an issue here
						tic = clock::now();
						do {
							toc = clock::now();
							dt = (float) std::chrono::duration_cast<std::chrono::microseconds>(toc - tic).count();
						} while(dt < timeout);
						*/

					} else if(ch_busy == true and pr_acked == false) { // Randomize backoff, increment contention window (pr_cw).
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

						else if(ch_busy == false and pr_acked == false and backoff > 0) { // Decrement backoff
						backoff--;
						if(backoff <= 0) {
							backoff = 0;
							sensing_time = 0;
						} else {
							sensing_time = pr_slot_time;
						}

						if(pr_debug) std::cout << "Decrement backoff. Current value = " << sensing_time*backoff << " (us)." << std::endl;
					}*/

					if(!is_broadcast) tot_attempts++; // Persistent on broadcast frames
				}

				if(pr_acked) { // Sucessful transmission. So, reset contention window.
					pr_cw = aCWmin;
					if(pr_debug) std::cout << "Frame acked properly!" << std::endl << std::flush;
				} else if(attempts >= MAX_RETRIES) {
					if(pr_debug) std::cout << "Max number of retries exceeded. Frame dropped!" << std::endl << std::flush;
				} else if(tot_attempts == MAX_RETRIES) {
					if(pr_debug) std::cout << "Medium is too busy. Frame dropped!" << std::endl << std::flush;
				}

				pr_acked = false;
				pr_buff.pop_front();
			}
		}

		bool is_channel_busy(int threshold, int time) {
			if(pr_debug) std::cout << "Request carrier sensing for " << time << " (us)." << std::endl;

			message_port_pub(msg_port_request_to_cs, pmt::string_to_symbol(std::to_string((float)time)));

			pr_sensing = true; 
			boost::unique_lock<boost::mutex> lock(pr_mu1);
			while(pr_sensing) pr_cs_cond.wait(lock);

			if(pr_debug) std::cout << "Avg power from medium = " << pr_avg_power << " (dB)." << std::endl;

			if(pr_avg_power < threshold) {
				return false;
			} else {
				return true;
			}
		}

		void frame_from_phy(pmt::pmt_t frame) {
			pmt::pmt_t cdr = pmt::cdr(frame);
			mac_header *h = (mac_header*)pmt::blob_data(cdr);

			bool is_mine, is_broadcast;
			if(memcmp(h->addr1, pr_broadcast_addr, 6) == 0) {
				is_broadcast = true;
			} else {
				is_broadcast = false;
			}
			if(memcmp(h->addr1, pr_mac_addr, 6) == 0) {
				is_mine = true;
			} else {
				is_mine = false;
			}

			if(!is_mine and !is_broadcast) {
				if(pr_debug) std::cout << "This frame is not for me. Drop it!" << std::endl << std::flush;
				return;
			}

			switch(h->frame_control) {
				case FC_DATA: {
					if(is_mine) {
						if(pr_debug) std::cout << "Data frame belongs to me. Ack sent!" << std::endl << std::flush;
						pmt::pmt_t ack = generate_ack_frame(frame);
						message_port_pub(msg_port_frame_to_phy, ack);
						message_port_pub(msg_port_frame_to_app, frame);
					}
				} break;

				case FC_ACK: {
					if(is_mine and h->seq_nr == pr_frame_seq_nr and pr_buff.size() > 0) {
						pr_acked = true;
						if(pr_debug) std::cout << "Ack for me!" << std::endl << std::flush;
					}			
				} break;
					
				case FC_METRICS: {
					if(is_mine) {
						pmt::pmt_t ack = generate_ack_frame(frame);
						message_port_pub(msg_port_frame_to_phy, ack);
					}
				} break;

				case FC_PROTOCOL: {
					// TODO
				} break;

				default: {
					if(pr_debug) std::cout << "Unkown frame type." << std::endl << std::flush;
					return;
				}
			}
		}

		void cs_in(pmt::pmt_t msg) {
			pr_avg_power = pmt::to_float(msg);
			
			pr_sensing = false;
			pr_cs_cond.notify_all();
		}

		pmt::pmt_t generate_ack_frame(pmt::pmt_t frame) {
			pmt::pmt_t car_frame = pmt::car(frame);
			pmt::pmt_t cdr_frame = pmt::cdr(frame);

			//size_t frame_len = pmt::blob_length(cdr_frame); // Length in bytes
			size_t frame_len = 24; // Length in bytes. Copy only the MAC header, first 24 bytes

			mac_header *frame_header = (mac_header*)pmt::blob_data(cdr_frame);

			// This avoids modifying frame while generating ack. So info is not lost when sent to app.
			mac_header ack_header;
			ack_header.frame_control = 0x2B00; // Stands for ACK on the Frame Control
			ack_header.duration = frame_header->duration;
			ack_header.seq_nr = frame_header->seq_nr;

			/* Update mac addresses */
			memcpy(ack_header.addr1, frame_header->addr2, 6);
			memcpy(ack_header.addr2, pr_mac_addr, 6);
			memcpy(ack_header.addr3, frame_header->addr3, 6);

			/* Calculate new checksum */
			boost::crc_32_type result;
			result.process_bytes(&ack_header, frame_len);
			uint32_t fcs = result.checksum();

			uint8_t psdu[1528];
			memcpy(psdu, &ack_header, frame_len);
			memcpy(psdu + frame_len, &fcs, sizeof(uint32_t));

			// header size is 24 and fcs size is 4 bytes, total = 28;
			pmt::pmt_t ack = pmt::make_blob(psdu, frame_len + sizeof(uint32_t));
			/* Done! Checksum updated. */

			// dict
			pmt::pmt_t dict = pmt::make_dict();
			dict = pmt::dict_add(dict, pmt::mp("crc_included"), pmt::PMT_T);

			if (pr_debug) std::cout << "Seq num of rx frame = " << frame_header->seq_nr << std::endl;

			return pmt::cons(dict, ack);
		}

	private:
		int pr_slot_time, pr_sifs, pr_difs, pr_frame_id, pr_alpha, pr_threshold;
		uint pr_cw;
		bool pr_debug, pr_sensing, pr_acked;
		float pr_avg_power;
		boost::condition_variable pr_cs_cond, pr_new_frame_cond;
		boost::mutex pr_mu0, pr_mu1;
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
		uint8_t pr_mac_addr[6], pr_broadcast_addr[6];

		// Frame to be sent
		uint16_t pr_frame_seq_nr;

		// Local buffer
		boost::circular_buffer<pmt::pmt_t> pr_buff;
};

csma_ca::sptr
csma_ca::make(std::vector<uint8_t> src_mac, int slot_time, int sifs, int difs, int alpha, int threshold, bool debug) {
	return gnuradio::get_initial_sptr(new csma_ca_impl(src_mac, slot_time, sifs, difs, alpha, threshold, debug));
}
