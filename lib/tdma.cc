/* -*- c++ -*- */
/* 
 * Copyright 2018 - André Gomes, The Federal University of Minas Gerias (UFMG) <andre.gomes@dcc.ufmg.br>.
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.	If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "tdma.h"
#include <boost/thread.hpp>
#include <unistd.h>
#include <string>
#include <cstdlib>
#include <stdlib.h>
#include <time.h>
#include <boost/crc.hpp>
#include <chrono>

#define MAX_RETRIES 5
#define MAX_NUM_STATIONS 32 // This number may change
#define AVG_BLOCK_DELAY 10 // (us)
// Frame Control (FC) cheat sheet
#define FC_ACK 0x2B00
#define FC_DATA 0x0008
// FC reserved to TDMA
#define FC_SYNC 0x2000 // Frame Control for SYNC
#define FC_ALLOC 0x2800 // Frame Control for Allocation
#define FC_REQ 0x2400 // Frame Control for Requesting a slot during allocation
#define FC_SKIP 0x2C00 // Frame Control for Skipping a slot during allocation (slot not allocated)
// Informs the protocol in use on the network
#define FC_PROTOCOL 0x2900 // Active protocol on network

using namespace gr::macprotocols;

class tdma_impl : public tdma {

	typedef std::chrono::high_resolution_clock clock;

	public:
		tdma_impl(bool is_coord, std::vector<uint8_t> src_mac, uint16_t slot_time, uint16_t alpha, bool debug) : gr::block("tdma",
							gr::io_signature::make(0, 0, 0),
							gr::io_signature::make(0, 0, 0)),
							pr_is_coord(is_coord), pr_slot_time(alpha*slot_time), pr_debug(debug),
								pr_sync_time(2*alpha*slot_time), pr_data_time(4*alpha*slot_time), pr_ack_time(2*alpha*slot_time) , pr_alloc_slot(2*alpha*slot_time), 
								pr_status(false), pr_frame_acked(false), pr_num_active_addr(0), pr_num_alloc_addr(0), pr_sync_time0(clock::now()), pr_comm_time0(clock::now()),
								pr_tx_order(0), pr_comm_started(false) {
			// Inputs
			message_port_register_in(msg_port_frame_from_buff);
			set_msg_handler(msg_port_frame_from_buff, boost::bind(&tdma_impl::frame_from_buff, this, _1));

			message_port_register_in(msg_port_frame_from_phy);
			set_msg_handler(msg_port_frame_from_phy, boost::bind(&tdma_impl::frame_from_phy, this, _1));

			// Outputs
			message_port_register_out(msg_port_frame_to_phy);
			message_port_register_out(msg_port_frame_request);
			message_port_register_out(msg_port_frame_to_app);

			for(int i = 0; i < 6; i++) {
				pr_mac_addr[i] = src_mac[i];
				pr_broadcast_addr[i] = 0xff;
			}

			pr_comm_slot = pr_data_time + pr_ack_time;
			pr_guard_time = 0.1*pr_comm_slot;
		}

		bool start() {
			/*
			 * The use of start() prevents the thread bellow to access the message port in check_buff() before it even exists. 
			 * This ensures the scheduler first deals with the msg port, then the thread is created.
			*/
			if(pr_is_coord) thread_sync = boost::shared_ptr<gr::thread::thread> (new gr::thread::thread(boost::bind(&tdma_impl::sync_func, this)));
			thread_send_frame = boost::shared_ptr<gr::thread::thread> (new gr::thread::thread(boost::bind(&tdma_impl::send_frame, this)));
			thread_check_buff = boost::shared_ptr<gr::thread::thread> (new gr::thread::thread(boost::bind(&tdma_impl::check_buff, this)));
			return block::start();
		}

		void check_buff() {
			// TODO: Find a more efficient way to get data from buffer only if block is iddle and buffer not empty
			while(true) {
				if(pr_status) { // Frame buffer is probably not empty, mind the queue carefully
					boost::unique_lock<boost::mutex> lock(pr_mu5);
					while(pr_status) pr_cond3.wait(lock); // send_frame has already a frame, wait until it gets available
					message_port_pub(msg_port_frame_request, pmt::string_to_symbol("get frame"));
					usleep(pr_comm_slot + pr_sync_time);
				} else { // This means no frame has arrived to be sent. So, it will request one to buffer.
					message_port_pub(msg_port_frame_request, pmt::string_to_symbol("get frame"));
					usleep((rand() % 5)*(pr_slot_time + pr_sync_time + pr_data_time) + AVG_BLOCK_DELAY); srand(time(NULL));
				}
			}
		}

		void frame_from_buff(pmt::pmt_t frame) {
			boost::unique_lock<boost::mutex> lock(pr_mu1);
			if(pr_debug) std::cout << "New frame from app" << std::endl << std::flush;

			if(!pr_status) {
				pr_frame_acked = false;
				pr_comm_started = false;
				pr_frame = frame; // Frame to be sent.
				pr_status = true; // This means that there is already one frame to be sent. No more frames will be requested meanwhile.
				pr_cond2.notify_all();
			}

			lock.unlock();
		}

		void send_frame() {
			decltype(clock::now()) toc;
			float elapsed_time, tx_time0;
			int count_tx;	

			while(true) {
				// Waiting for a new frame
				boost::unique_lock<boost::mutex> lock0(pr_mu4);
				while(!pr_status) pr_cond2.wait(lock0);

				// Register frame sequence number
				pmt::pmt_t cdr = pmt::cdr(pr_frame);
				mac_header *h = (mac_header*)pmt::blob_data(cdr);
				pr_frame_seq_nr = h->seq_nr;
				count_tx = 0;

				// Attempt to transmit
				while(!pr_frame_acked and count_tx < MAX_RETRIES) {
					boost::unique_lock<boost::mutex> lock1(pr_mu2);
					while(!pr_comm_started) pr_cond1.wait(lock1); // Waits for the beggining of Communication Interval

					do { // Waits for the beginning of the allocated comm slot
						toc = clock::now();
						elapsed_time = (float) std::chrono::duration_cast<std::chrono::microseconds>(toc - pr_comm_time0).count();
						tx_time0 = pr_tx_order*pr_comm_slot;
					} while(elapsed_time < tx_time0 and !pr_frame_acked);

					if(elapsed_time - tx_time0 <= pr_guard_time and !pr_frame_acked) { // Guarantees transmission will be done within the correct comm slot
						message_port_pub(msg_port_frame_to_phy, pr_frame);
						count_tx++;
						if(pr_debug) std::cout << "Transmitting frame for the " << count_tx << "th time." << std::endl << std::flush;

						if(memcmp(h->addr1, pr_broadcast_addr, 6) == 0) { // Broadcast frame, no ACK expected
							pr_frame_acked = true;
							if(pr_debug) std::cout << "Broadcast frame was sent!" << std::endl << std::flush;
						}
					}

					pr_comm_started = false; // Anyway, wait until next communication interval
				}
				if(pr_debug and count_tx >= MAX_RETRIES) std::cout << "Max number of retries exceeded. Drop frame!" << std::endl << std::flush;

				pr_status = false; // A new frame from buffer may arrive for transmission.
				pr_frame_acked = false;
				pr_cond3.notify_all();
			}
		}

		void frame_from_phy(pmt::pmt_t frame) {
			pmt::pmt_t cdr = pmt::cdr(frame);
			mac_header *h = (mac_header*)pmt::blob_data(cdr);

			int is_broadcast = memcmp(h->addr1, pr_broadcast_addr, 6); // 0 if equal (is_broadcast is TRUE)
			int is_mine = memcmp(h->addr1, pr_mac_addr, 6); // 0 if equal (is_mine is TRUE)

			if(is_mine != 0 and is_broadcast != 0) {
				if(pr_debug) std::cout << "This frame is not for me. Drop it!" << std::endl << std::flush;
				return;
			}

			switch(h->frame_control) {
				case FC_DATA: { // Data frame
					if(is_mine == 0) {
						if(pr_debug) std::cout << "Data frame belongs to me. Ack sent!" << std::endl << std::flush;
						pmt::pmt_t ack = generate_ack_frame(frame);
						message_port_pub(msg_port_frame_to_phy, ack);
						message_port_pub(msg_port_frame_to_app, frame);
					}
				} break;

				case FC_ACK: { // ACK frame
					if(is_mine == 0) {
						if(pr_debug) std::cout << "ACK for me!" << std::endl << std::flush;

						if((h->seq_nr == pr_frame_seq_nr) and !pr_frame_acked and pr_status) { // This means I'm waiting for this ack (Right seq_nr, not acked yet and it has not been dropped).
							pr_frame_acked = true;
							if(pr_debug) std::cout << "Frame was acked properly!" << std::endl << std::flush;
						}
					}
				} break;

				case FC_SYNC: { // SYNC frame
					if(!pr_is_coord and is_broadcast == 0) { // Normal node. Coordinator handles this on sync_func.
						// Beginning of super frame
						pr_sync_time0 = clock::now();
						if(pr_debug) std::cout << "Beginning of super frame." << std::endl << std::flush;

						uint8_t msdu[0]; pmt::pmt_t resp_frame; // Response frame

						// Is there anything to transmit?
						if(pr_status) { // Yes, there is a frame to be transmitted. So, request comm slot.
							resp_frame = generate_frame(msdu, 0, FC_REQ, 0x0000, h->addr2);
						} else { // No, send SKIP comm slot.
							resp_frame = generate_frame(msdu, 0, FC_SKIP, 0x0000, h->addr2);
						}

						// Identifying order for transmitting resp frame.
						uint8_t *f = (uint8_t*)pmt::blob_data(cdr);
						int msg_len = pmt::blob_length(cdr) - 24; // Strips header
						uint8_t msg[msg_len];
						memcpy(msg, f + 24, msg_len);

						int tx_order = -1;
						for(int i = 0; i <= msg_len - 6; i = i + 6) { // Checks all addrs if any is mine.
							if(memcmp(pr_mac_addr, msg + i, 6) == 0) { // My addr is here!
								tx_order = i/6;
							}
						}
						if(tx_order == -1) tx_order = msg_len/6; // This means node is not listed. Therefore, it must transmit at the last alloc slot.

						float tx_time0 = pr_sync_time + tx_order*pr_alloc_slot;
						auto toc = clock::now();
						float elapsed_time = (float) std::chrono::duration_cast<std::chrono::microseconds>(toc - pr_sync_time0).count();
						while(elapsed_time < tx_time0) {
							toc = clock::now();
							elapsed_time = (float) std::chrono::duration_cast<std::chrono::microseconds>(toc - pr_sync_time0).count();
						}
						if(pr_debug) std::cout << "tx_time0 (theory) = " << tx_time0 << "us, tx_time0 (actual) = " << elapsed_time << "us." << std::endl << std::flush;

						if(elapsed_time < tx_time0 + pr_alloc_slot) {
							message_port_pub(msg_port_frame_to_phy, resp_frame);
							if(pr_debug) std::cout << "REQ comm slot was sent!" << msg_len/6 << " active nodes are active on the network.";
						}
					}
				} break;

				case FC_REQ: { // Someone is requesting a slot allocation
					if(pr_is_coord and is_mine == 0) {
						boost::unique_lock<boost::mutex> lock(pr_mu1);
						if(pr_debug) std::cout << "A node has requested a comm slot" << std::endl << std::flush;
						
						memcpy(pr_alloc_addrs + pr_num_alloc_addr*6, h->addr2, 6);
						pr_num_alloc_addr++;

						memcpy(pr_active_addrs + pr_num_active_addr*6, h->addr2, 6);
						pr_num_active_addr++;

						lock.unlock();
					}
				} break;

				case FC_SKIP: { // Someone is skipping the slot allocation
					if(pr_is_coord and is_mine == 0) {
						boost::unique_lock<boost::mutex> lock(pr_mu1);
						if(pr_debug) std::cout << "A node has skipped a comm slot" << std::endl << std::flush;
						
						memcpy(pr_active_addrs + pr_num_active_addr*6, h->addr2, 6);
						pr_num_active_addr++;

						lock.unlock();
					}
				} break;

				case FC_ALLOC: { // This figures out which comm slot is allocated for this node.
					if(!pr_is_coord and is_broadcast == 0 and pr_status) {
						if(pr_debug) std::cout << "ALLOC frame has arrived. Figuring out order." << std::endl << std::flush;

						// Identifying order for transmitting resp frame.
						uint8_t *f = (uint8_t*)pmt::blob_data(cdr);
						int msg_len = pmt::blob_length(cdr) - 24; // Strips header
						uint8_t msg[msg_len];
						memcpy(msg, f + 24, msg_len);

						int tx_order = -1;
						for(int i = 0; i <= msg_len - 6; i = i + 6) { // Checks all addrs if any is mine.
							if(memcmp(pr_mac_addr, msg + i, 6) == 0) { // My addr is here!
								tx_order = i/6;
							}
						}
						if(tx_order >= 0) { // Beginning of Communication Interval
							pr_comm_time0 = clock::now();
							pr_tx_order = tx_order;
							pr_comm_started = true; 
							pr_cond1.notify_all();

							if(pr_debug) std::cout << "COMM Slot was allocated. Node will be the " << pr_tx_order << "th to transmit." << std::endl << std::flush;
						} else if (tx_order == -1) if(pr_debug) std::cout << "No COMM Slot allocated to me. Waiting for next super frame..." << std::endl << std::flush;
					}
				} break; 

				case FC_PROTOCOL: { // Get the active protocol on network
					// TODO
				} break;

				default: {
					if(pr_debug) std::cout << "Unkown frame type." << std::endl << std::flush;
					return;
				}
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

		void sync_func() { // Only Coordinator
			// This function dictates the beginning of all super frames. It sends the SYNC and ALLC messages
			if(pr_debug) std::cout << "Starting SYNC Function, coordinator mode ON" << std::endl << std::flush;
			
			uint8_t msdu[6*MAX_NUM_STATIONS];
			int msdu_size;
			decltype(clock::now()) toc;
			float waiting_time, elapsed_time;
			pmt::pmt_t sync_frame, alloc_frame;
			while(true) {
				// Building the SYNC frame
				msdu_size = pr_num_active_addr*6; // Each active node has a 6 bytes long address
				memcpy(msdu, pr_active_addrs, msdu_size);

				// Sending SYNC Frame Control
				sync_frame = generate_frame(msdu, msdu_size, FC_SYNC, 0x0000, pr_broadcast_addr);
				message_port_pub(msg_port_frame_to_phy, sync_frame);
				pr_sync_time0 = clock::now(); // Beginning of Allocation Interval

				// Wait the end of allocation interval. +1 alloc slot is reserved for new nodes.
				waiting_time = pr_sync_time + (pr_num_active_addr + 1)*pr_alloc_slot;
				if(pr_debug) std::cout << "pr_sync_time = " << pr_sync_time << ", pr_num_active_addr = " << pr_num_active_addr << std::endl << std::flush;
				pr_num_active_addr = 0; // Reseting counters. Active nodes will report themselves during "waiting_time".
				pr_num_alloc_addr = 0;
				do {
					toc = clock::now();
					elapsed_time = (float) std::chrono::duration_cast<std::chrono::microseconds>(toc - pr_sync_time0).count();
				} while(elapsed_time < waiting_time);
				if(pr_debug) std::cout << "SYN time (theory) = " << waiting_time << "us, SYN time (actual) = " << elapsed_time << "us." << std::endl << std::flush;

				// Alloc interval is about to end. The last thing is to send the ordered list of addr that requested a comm slot.
				boost::unique_lock<boost::mutex> lock(pr_mu3);
				if(pr_debug) std::cout << "Number of active nodes = " << pr_num_active_addr << ", number of requested comm slots = " << pr_num_alloc_addr << std::endl << std::flush;
				msdu_size = pr_num_alloc_addr*6;
				memcpy(msdu, pr_alloc_addrs, msdu_size);
				lock.unlock();

				alloc_frame = generate_frame(msdu, msdu_size, FC_ALLOC, 0x0000, pr_broadcast_addr);
				message_port_pub(msg_port_frame_to_phy, alloc_frame);

				// TODO: check if coordinator has something to transmitting before allocating its comm slot
				// Beginning of Communication Interval
				pr_comm_time0 = clock::now();
				pr_tx_order = pr_num_active_addr; // Time when comm slot beggins for coordinator (last one).
				pr_comm_started = true; 
				pr_cond1.notify_all();
				if(pr_debug) std::cout << "COMM Slot was allocated. Node will be the " << pr_tx_order << "th to transmit." << std::endl << std::flush;
				
				// Wait before starting another super frame
				waiting_time = pr_comm_slot*(pr_num_active_addr + 1); // The last one is reserved for coordinator.
				usleep(waiting_time); // It seems this one does not need too much accuracy. Change to busy waiting if necessary. (Uncomment bellow)
				/* Busy waiting
				do {
					toc = clock::now();
					duration = duration = (float) std::chrono::duration_cast<std::chrono::microseconds>(toc - pr_sync_time0).count();
				} while(duration < waiting);
				*/
			}
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
		// Input parameters
		int pr_slot_time;
		bool pr_is_coord, pr_debug;

		// Internal parameters & variables
		int pr_sync_time, pr_data_time, pr_ack_time, pr_alloc_slot, pr_comm_slot;
		bool pr_status, pr_frame_acked, pr_comm_started;

		uint8_t pr_active_addrs[6*MAX_NUM_STATIONS], pr_alloc_addrs[6*MAX_NUM_STATIONS]; // Active nodes, Nodes that requested comm slot
		int pr_num_active_addr, pr_num_alloc_addr;
		decltype(clock::now()) pr_sync_time0, pr_comm_time0; // This times the beggining of SYNC frame
		float pr_tx_order; // Order for transmitting during communication interval
		float pr_guard_time; // Accepted "error" time during transmission synchronization

		// MAC addr
		uint8_t pr_mac_addr[6], pr_broadcast_addr[6];

		// Input ports
		pmt::pmt_t msg_port_frame_from_buff = pmt::mp("frame from buffer");
		pmt::pmt_t msg_port_frame_from_phy = pmt::mp("frame from phy");

		// Output ports
		pmt::pmt_t msg_port_frame_to_phy = pmt::mp("frame to phy");
		pmt::pmt_t msg_port_frame_request = pmt::mp("frame request");
		pmt::pmt_t msg_port_frame_to_app = pmt::mp("frame to app");

		// Mutex & Threads & Cond variables
		boost::mutex pr_mu1, pr_mu2, pr_mu3, pr_mu4, pr_mu5;
		boost::condition_variable pr_cond1, pr_cond2, pr_cond3;
		boost::shared_ptr<gr::thread::thread> thread_check_buff, thread_send_frame, thread_sync;

		// Frame to be sent
		pmt::pmt_t pr_frame;
		uint16_t pr_frame_seq_nr;
};


tdma::sptr
tdma::make(bool is_coord, std::vector<uint8_t> src_mac, uint16_t slot_time, uint16_t alpha, bool debug) {
	return gnuradio::get_initial_sptr(new tdma_impl(is_coord, src_mac, slot_time, alpha, debug));
}