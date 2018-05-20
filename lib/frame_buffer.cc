/* -*- c++ -*- */
/* 
 * Copyright 2018 André Gomes, UFMG - <andre.gomes@dcc.ufmg.br>.
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

/* -*- -*- Notes on Frame Buffer -*- -*-
 	This block has been extended in order to support a further project. If
you are interested only in the MAC protocols, ignore ports/functions
"ctrl in", "broad in", "req in +1" and "frame out +1". They are not 
relevant to gr-macprotocols at all.
	Look up ARP is useful if you intend to use the tun/tap feature. Originally,
the project ieee802_11 is embedded with the block WIFI MAC. It is responsible
for constructing the Data Link frame. However, it uses a fix MAC addr as 
destination. Look up ARP changes the destination MAC addr acoording to 
the ARP table. This is meant to run in linux based system, where ARP table
is locatted at /proc/net/arp.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "frame_buffer.h"
#include <string.h>
#include <boost/crc.hpp>
#include <boost/circular_buffer.hpp>
#include <pmt/pmt.h>

// ARP table path
#define ARP_CACHE "/proc/net/arp"

using namespace gr::macprotocols;

class frame_buffer_impl : public frame_buffer {
	public:

		frame_buffer_impl(int buff_size, bool arp, uint8_t portid, bool debug)
			: gr::block("frame_buffer",
							gr::io_signature::make(0, 0, 0),
							gr::io_signature::make(0, 0, 0)),
							pr_buff_size(buff_size), pr_arp(arp), pr_portid(portid), pr_debug(debug){

			// Init buffer
			pr_circ_buff.rset_capacity(pr_buff_size);
			std::cout << "Buffer capacity = " << pr_circ_buff.capacity() << std::endl << std::flush;

			// Inpurt msg ports
			message_port_register_in(msg_port_appin);
			set_msg_handler(msg_port_appin, boost::bind(&frame_buffer_impl::appin, this, _1));

			message_port_register_in(msg_port_ctrlin);
			set_msg_handler(msg_port_ctrlin, boost::bind(&frame_buffer_impl::ctrlin, this, _1));

			message_port_register_in(msg_port_req0);
			set_msg_handler(msg_port_req0, boost::bind(&frame_buffer_impl::req0, this, _1));

			message_port_register_in(msg_port_req1);
			set_msg_handler(msg_port_req1, boost::bind(&frame_buffer_impl::req1, this, _1));

			message_port_register_in(msg_port_req2);
			set_msg_handler(msg_port_req2, boost::bind(&frame_buffer_impl::req2, this, _1));

			message_port_register_in(msg_port_broad);
			set_msg_handler(msg_port_broad, boost::bind(&frame_buffer_impl::broad, this, _1));

			message_port_register_in(msg_port_metrics);
			set_msg_handler(msg_port_metrics, boost::bind(&frame_buffer_impl::metrics, this, _1));

			// Output msg ports
			message_port_register_out(msg_port_frame0);
			message_port_register_out(msg_port_frame1);
			message_port_register_out(msg_port_frame2);
		}

		void appin(pmt::pmt_t frame) {
			if(pr_arp) {
				uint8_t ip_dst[4], mac[6];
				pmt::pmt_t cdr = pmt::cdr(frame);
				uint8_t *f = (uint8_t*) pmt::blob_data(cdr);
				int frame_len = pmt::blob_length(cdr);
				memcpy(ip_dst, f + 48, 4);

				if(lookup_arp(ip_dst, mac)) {
					if(pr_debug) std::cout << "Dest MAC = ";
					for(int i = 0; i < 5; i++) if(pr_debug) std::cout << +mac[i] << ":";
					if(pr_debug) std::cout << +mac[5] << std::endl << std::flush;

					memcpy(f + 4, mac, 6);
					boost::crc_32_type result;
					result.process_bytes(f, frame_len - 4);
					uint32_t fcs = result.checksum();

					memcpy(f + frame_len - 4, &fcs, sizeof(uint32_t));

					cdr = pmt::make_blob(f, frame_len);
					pmt::pmt_t car = pmt::make_dict();
					car = pmt::dict_add(car, pmt::mp("crc_included"), pmt::PMT_T);

					frame = pmt::cons(car, cdr);
				} else {
					if(pr_debug) std::cout << "Unknown dest MAC" << std::endl << std::flush;
				}
			}

			if(pr_circ_buff.size() == pr_buff_size)
				if(pr_debug) std::cout << "BUFFER IS FULL!" << std::endl << std::flush;

			pr_circ_buff.push_back(frame);
		}

		bool lookup_arp(uint8_t *ip, uint8_t *mac) {
			char line[256], buff[256];
			int intip[4], intmac[6];
			uint8_t int8ip[4];

			FILE *fp = fopen(ARP_CACHE, "r");
			if(fgets(line, sizeof(line), fp)) {} // Skips first line

			while(fgets(line, sizeof(line), fp)) {
				sscanf(line, "%d.%d.%d.%d %s %s %x:%x:%x:%x:%x:%x %s\n",
					&intip[0], &intip[1], &intip[2], &intip[3], buff, buff + 32,
					&intmac[0], &intmac[1], &intmac[2], &intmac[3], &intmac[4], &intmac[5], buff + 64);

				for(int i = 0; i < 4; i++) int8ip[i] = (uint8_t) intip[i];
				if(memcmp(ip, int8ip, 4) == 0) {
					for(int i = 0; i < 6; i++) mac[i] = (uint8_t) intmac[i];
					fclose(fp);
					return true;
				}
			}
			fclose(fp);
			return false;
		}

		void ctrlin(pmt::pmt_t ctrl_msg) {
			std::string str = pmt::symbol_to_string(ctrl_msg);
			
			if(str == "portid-1") pr_portid = -1; // This means there is no chosen protocol
			else if(str == "portid0") pr_portid = 0;
			else if(str == "portid1") pr_portid = 1;
			else if(str == "portid2") pr_portid = 2;
			else return;

			if(pr_debug) std::cout << "Port id = " << (int)pr_portid << std::endl << std::flush;
		}

		// REQUEST PORTS

		void req0(pmt::pmt_t msg) {
			std::string str = pmt::symbol_to_string(msg);
			if(str == "get frame" and pr_portid == 0) {
				if(pr_circ_buff.size() > 0) {
					message_port_pub(msg_port_frame0, pr_circ_buff[0]);
					pr_circ_buff.pop_front();
					if(pr_debug) std::cout << "Frame was sent from BUFFER. Buffer size = " << pr_circ_buff.size() << std::endl << std::flush;
				}
			}
		}

		void req1(pmt::pmt_t msg) {
			std::string str = pmt::symbol_to_string(msg);
			if(str == "get frame" and pr_portid == 1) {
				if(pr_circ_buff.size() > 0) {
					message_port_pub(msg_port_frame1, pr_circ_buff[0]);
					pr_circ_buff.pop_front();
					if(pr_debug) std::cout << "Frame was sent from BUFFER. Buffer size = " << pr_circ_buff.size() << std::endl << std::flush;
				}
			}
		}

		void req2(pmt::pmt_t msg) {
			std::string str = pmt::symbol_to_string(msg);
			if(str == "get frame" and pr_portid == 2) {
				if(pr_circ_buff.size() > 0) {
					message_port_pub(msg_port_frame2, pr_circ_buff[0]);
					pr_circ_buff.pop_front();
					if(pr_debug) std::cout << "Frame was sent from BUFFER. Buffer size = " << pr_circ_buff.size() << std::endl << std::flush;
				}
			}
		}

		void broad(pmt::pmt_t broad_frame) { // Broadcasting frame, it bypasses the queue
			pr_circ_buff.push_front(broad_frame);
		}

		void metrics(pmt::pmt_t metrics_frame) { // Metrics' frame, it bypasses the queue
			pr_circ_buff.push_front(metrics_frame);
		}

	private:
		// Internal variables
		bool pr_arp, pr_debug;
		uint8_t pr_portid;

		// Buffer from upper layer
		int pr_buff_size;
		boost::circular_buffer<pmt::pmt_t> pr_circ_buff;

		// Input ports
		pmt::pmt_t msg_port_appin = pmt::mp("app in");
		pmt::pmt_t msg_port_ctrlin = pmt::mp("ctrl in");
		pmt::pmt_t msg_port_req0 = pmt::mp("req in 0");
		pmt::pmt_t msg_port_req1 = pmt::mp("req in 1");
		pmt::pmt_t msg_port_req2 = pmt::mp("req in 2");
		pmt::pmt_t msg_port_broad = pmt::mp("broad in");
		pmt::pmt_t msg_port_metrics = pmt::mp("metrics in");

		// Output ports
		pmt::pmt_t msg_port_frame0 = pmt::mp("frame out 0");
		pmt::pmt_t msg_port_frame1 = pmt::mp("frame out 1");
		pmt::pmt_t msg_port_frame2 = pmt::mp("frame out 2");
};

frame_buffer::sptr
frame_buffer::make(int buff_size, bool arp, uint8_t portid, bool debug) {
			return gnuradio::get_initial_sptr(new frame_buffer_impl(buff_size, arp, portid, debug));
}
