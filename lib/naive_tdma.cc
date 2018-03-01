/* -*- c++ -*- */
/* 
 * Copyright 2018 <+YOU OR YOUR COMPANY+>.
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "naive_tdma.h"
#include <boost/thread.hpp>
#include <chrono>
#include <boost/crc.hpp>
#include <boost/circular_buffer.hpp>

#define MAX_NUM_NODES 64
#define PHY_DELAY 50000 // 10ms, mostly on Gnu Radio (empirical)
#define MAX_RETRIES 5
#define MAX_LOCAL_BUFF 3
#define AVG_BLOCK_DELAY 1000 // us, so 1ms

#define FC_ACK 0x2B00
#define FC_DATA 0x0008
// FC reserved to TDMA
#define FC_SYNC 0x2000 // Frame Control for SYNC
#define FC_SKIP 0x2C00 // If there is no frame to be transmitted, send a skip frame instead
// Informs the protocol in use on the network
#define FC_PROTOCOL 0x2900 // Active protocol on network

using namespace gr::macprotocols;

class naive_tdma_impl : public naive_tdma {
	typedef std::chrono::high_resolution_clock clock;

	public:
		naive_tdma_impl(bool is_coord, std::vector<uint8_t> src_mac, int slot_time, int alpha, bool debug)
			: gr::block("naive_tdma",
			gr::io_signature::make(0, 0, 0),
			gr::io_signature::make(0, 0, 0)),
			pr_is_coord(is_coord), pr_debug(debug),  pr_slot_time(alpha * slot_time){

			pr_act_nodes_count = 0;

			pr_sync_time = 2*pr_slot_time;
			pr_ack_time = pr_slot_time;
			pr_comm_time = 2*pr_slot_time + pr_ack_time; // Full communication time slot for each node
			
			for(int i = 0; i < 6; i++) {
				pr_mac_addr[i] = src_mac[i];
				pr_broadcast_addr[i] = 0xff;
			}

			pr_acked = true;

			// Inputs
			message_port_register_in(msg_port_frame_from_buff);
			set_msg_handler(msg_port_frame_from_buff, boost::bind(&naive_tdma_impl::frame_from_buff, this, _1));

			message_port_register_in(msg_port_frame_from_phy);
			set_msg_handler(msg_port_frame_from_phy, boost::bind(&naive_tdma_impl::frame_from_phy, this, _1));

			// Outputs
			message_port_register_out(msg_port_frame_to_phy);
			message_port_register_out(msg_port_frame_request);
			message_port_register_out(msg_port_frame_to_app);

			// Set local buffer capacity, should be the smallest possible
			pr_buff.rset_capacity(MAX_LOCAL_BUFF);
		}

		bool start() {
			if(pr_is_coord) thread_sync = boost::shared_ptr<gr::thread::thread> (new gr::thread::thread(boost::bind(&naive_tdma_impl::sync_func, this)));
			thread_send_frame = boost::shared_ptr<gr::thread::thread> (new gr::thread::thread(boost::bind(&naive_tdma_impl::send_frame, this)));
			thread_check_buff = boost::shared_ptr<gr::thread::thread> (new gr::thread::thread(boost::bind(&naive_tdma_impl::check_buff, this)));

			return block::start();
		}

		void check_buff() {
			// TODO: Find a more efficient way to get data from buffer only if block is iddle and buffer not empty
			while(true) {
				if(pr_buff.size() < MAX_LOCAL_BUFF) {
					message_port_pub(msg_port_frame_request, pmt::string_to_symbol("get frame"));
					usleep(AVG_BLOCK_DELAY);
				} else {
					usleep((pr_sync_time + pr_comm_time) * (MAX_LOCAL_BUFF*0.8));
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
			pmt::pmt_t cdr;
			mac_header *h;
			int count;
			decltype(clock::now()) tnow;
			float wait_time, dt;
			bool is_broadcast;
			boost::unique_lock<boost::mutex> lock0(pr_mu0);
			boost::unique_lock<boost::mutex> lock1(pr_mu1);

			while(true) {
				// Waiting a new frame
				while(pr_buff.size() <= 0) pr_new_frame_cond.wait(lock0);

				cdr = pmt::cdr(pr_buff[0]);
				h = (mac_header*)pmt::blob_data(cdr);
				pr_frame_seq_nr = h->seq_nr;
				count = 0;

				if(memcmp(h->addr1, pr_broadcast_addr, 6) == 0) {
					is_broadcast = true;
				} else {
					is_broadcast = false;
				}

				// Attempt to transmit
				while(!pr_acked and count < MAX_RETRIES) {
					while(!pr_tx) pr_tx_cond.wait(lock1); // Has super frame started? Wait for it!
					pr_tx = false;

					if(pr_acked) break;
					
					// Wait for my comm slot for tx
					wait_time = pr_sync_time + pr_tx_order * pr_comm_time;
					do {
						tnow = clock::now();
						dt = (float) std::chrono::duration_cast<std::chrono::microseconds>(tnow - pr_sync0).count();
					} while(dt < wait_time);
					if(pr_debug) std::cout << "It should wait for " << wait_time << " us, it has waited for " << dt << std::endl << std::flush;

					if(!pr_acked) {
						message_port_pub(msg_port_frame_to_phy, pr_buff[0]);
						count++;

						if(is_broadcast) {
							pr_acked = true;
							if(pr_debug) std::cout << "A broadcast frame was sent" << std::endl << std::flush;
						}
					}
				}
				if(pr_debug and count >= MAX_RETRIES) std::cout << "Max # of attempts exceeded. Drop the frame!" << std::endl << std::flush;

				// Resetting counters
				pr_buff.pop_front();
				pr_acked = false;
			}
		}

		void send_skip_frame(uint8_t *dst_addr) {
			decltype(clock::now()) tnow;
			float dt;
			float wait_time = pr_sync_time + pr_tx_order * pr_comm_time;

			uint8_t *msdu;
			pmt::pmt_t skip_frame = generate_frame(msdu, 0, FC_SKIP, 0x0000, dst_addr);
			do {
				tnow = clock::now();
				dt = (float) std::chrono::duration_cast<std::chrono::microseconds>(tnow - pr_sync0).count();
			} while(dt < wait_time);

			message_port_pub(msg_port_frame_to_phy, skip_frame);
			if(pr_debug) std::cout << "SKIP frame sent" << std::endl << std::flush; 
		}

		void frame_from_phy(pmt::pmt_t frame) {
			pmt::pmt_t cdr = pmt::cdr(frame);
			mac_header *h = (mac_header*)pmt::blob_data(cdr);

			bool is_broadcast, is_mine;

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

			// Coordinator keeps track of active nodes
			if(pr_is_coord and (memcmp(h->addr2, pr_mac_addr, 6) != 0) 
					and (h->frame_control == FC_DATA or h->frame_control == FC_SKIP)) {
				if(pr_debug) std::cout << "Recording active node on network" << std::endl << std::flush;
				memcpy(pr_act_nodes + pr_act_nodes_count * 6, h->addr2, 6);
				pr_act_nodes_count++;
			}

			// Discard frame
			if(!is_broadcast and !is_mine) {
				if(pr_debug) std::cout << "Neither mine nor broadcast" << std::endl << std::flush;
				return;
			}

			switch(h->frame_control) {
				case FC_DATA: {
					if(is_mine) {
						if(pr_debug) std::cout << "Data frame belongs to me. Ack sent!" << std::endl << std::flush;
						message_port_pub(msg_port_frame_to_phy, generate_ack_frame(frame));
						message_port_pub(msg_port_frame_to_app, frame);
					}
				} break;

				case FC_ACK: {
					if(is_mine) {
						if(pr_debug) std::cout << "ACK for me!" << std::endl << std::flush;

						if((h->seq_nr == pr_frame_seq_nr) and !pr_acked and (pr_buff.size() > 0)) {
							pr_acked = true;
							if(pr_debug) std::cout << "Frame was acked properly!" << std::endl << std::flush;
						}
					}
				} break;

				case FC_SYNC: {
					if(!pr_is_coord and is_broadcast) {
						if(pr_debug) std::cout << "Beginning of super frame." << std::endl << std::flush;
						pr_sync0 = clock::now();

						uint8_t *f = (uint8_t*)pmt::blob_data(cdr);
						int len = pmt::blob_length(cdr) - 24; // Strips header
						uint8_t msg[len];
						memcpy(msg, f + 24, len);

						// Find out transmission order
						int non = len/6; // Number of nodes
						pr_tx_order = non + 1; // In case this node is not listed, it will be the last one to transmit
						for(int i = 0; i < non; i++) {
							if(memcmp(pr_mac_addr, msg + i*6, 6) == 0) {
								pr_tx_order = i + 1;
								break;
							}
						}

						if(pr_buff.size() > 0) { // There is a frame to be transmitted
							pr_tx = true;
							pr_tx_cond.notify_all();
						} else { // No frame to be transmitted; transmit SKIP msg
							send_skip_frame(h->addr2);
						}
					}
				} break;

				case FC_SKIP: {
					if(is_mine) {
						if(pr_debug) std::cout << "Skip frame was received" << std::endl << std::flush;
					}
				} break; 

				case FC_PROTOCOL: {
					// TODO
				} break;

				default: {
					if(pr_debug) std::cout << "Unkown frame type." << std::endl << std::flush;
				} break;
			}
		}

		void sync_func() {
			uint8_t *msdu;
			int msdu_size; 
			pmt::pmt_t sync_frame;
			float sleep_time;

			pr_tx_order = 0; // First slot is always allocated to coordinator 
			while(true) {
				// Super frame has just started
				if(pr_debug) std::cout << "Super frame has just started!" << std::endl << std::flush;
				pr_sync0 = clock::now();

				/* Build sync frame. It holds the tx order.
					1st slot: coordinator (1st is the best for sync reasons)
					2nd slot: node1
					3rd slot: node2
					...
					i-th slot: reserved to new nodes
				*/
				msdu_size = 6 * pr_act_nodes_count;
				msdu = pr_act_nodes;
				sync_frame = generate_frame(msdu, msdu_size, FC_SYNC, 0x0000, pr_broadcast_addr);
				message_port_pub(msg_port_frame_to_phy, sync_frame);

				// Reset counters
				sleep_time = pr_sync_time + pr_comm_time * (pr_act_nodes_count + 2); // +2: 1 slot reserved to coord; 1 slot reserved to new nodes
				pr_act_nodes_count = 0;

				if(pr_buff.size() > 0) { 
					pr_tx = true;
					pr_tx_cond.notify_all();
				}
				// Wait until super frame ends to start a new one
				usleep(sleep_time + PHY_DELAY); // Accuracy should not be crucial here.
			}
		}

		pmt::pmt_t generate_ack_frame(pmt::pmt_t frame) {
			pmt::pmt_t car_frame = pmt::car(frame);
			pmt::pmt_t cdr_frame = pmt::cdr(frame);
			
			mac_header *header = (mac_header*)pmt::blob_data(cdr_frame);
			uint8_t msdu[0];
			pmt::pmt_t ack = generate_frame(msdu, 0, FC_ACK, header->seq_nr, header->addr2);

			if (pr_debug) std::cout << "Seq num of rx frame = " << header->seq_nr << std::endl;

			return ack;
		}

		pmt::pmt_t generate_frame(uint8_t *msdu, int msdu_size, uint16_t fc, uint16_t seq_nr, uint8_t *dst_addr) {
		// Inputs: (data, data size, frame control, sequence number, destination address)
		mac_header header;
		header.frame_control = fc;
		header.duration = 0x0000;
		header.seq_nr = seq_nr;

		memcpy(header.addr1, dst_addr, 6);
		memcpy(header.addr2, pr_mac_addr, 6);
		memcpy(header.addr3, pr_broadcast_addr, 6);

		uint8_t psdu[1528];
		memcpy(psdu, &header, 24); // Header is 24 bytes long
		memcpy(psdu + 24, msdu, msdu_size); 

		boost::crc_32_type result;
		result.process_bytes(&psdu, 24 + msdu_size);
		uint32_t fcs = result.checksum();

		memcpy(psdu + 24 + msdu_size, &fcs, sizeof(uint32_t));

		// Building frame from cdr & car
		pmt::pmt_t frame = pmt::make_blob(psdu, 24 + msdu_size + sizeof(uint32_t));
		pmt::pmt_t dict = pmt::make_dict();
		dict = pmt::dict_add(dict, pmt::mp("crc_included"), pmt::PMT_T);

		return pmt::cons(dict, frame);
	}

	private:
		// MAC addresses
		uint8_t pr_mac_addr[6], pr_broadcast_addr[6];

		// Variables
		int pr_slot_time, pr_ack_time, pr_sync_time, pr_comm_time, pr_tx_order;
		bool pr_is_coord, pr_debug, pr_acked, pr_tx;
		decltype(clock::now()) pr_sync0;
		uint8_t pr_act_nodes[6*MAX_NUM_NODES];
		int pr_act_nodes_count; 
		uint16_t pr_frame_seq_nr;

		// Output ports
		pmt::pmt_t msg_port_frame_to_phy = pmt::mp("frame to phy");
		pmt::pmt_t msg_port_frame_request = pmt::mp("frame request");
		pmt::pmt_t msg_port_frame_to_app = pmt::mp("frame to app");

		// Input ports
		pmt::pmt_t msg_port_frame_from_buff = pmt::mp("frame from buffer");
		pmt::pmt_t msg_port_frame_from_phy = pmt::mp("frame from phy");

		// Conditional variables
		boost::condition_variable pr_tx_cond, pr_new_frame_cond;

		// Locks
		boost::mutex pr_mu0, pr_mu1;

		// Threads
		boost::shared_ptr<gr::thread::thread> thread_check_buff, thread_send_frame, thread_sync;

		// Local buffer
		boost::circular_buffer<pmt::pmt_t> pr_buff;
};

naive_tdma::sptr
naive_tdma::make(bool is_coord, std::vector<uint8_t> src_mac, int slot_time, int alpha, bool debug) {
	return gnuradio::get_initial_sptr(new naive_tdma_impl(is_coord, src_mac, slot_time, alpha, debug));
}
