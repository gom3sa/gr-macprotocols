/* -*- c++ -*- */
/* 
 * Copyright 2017 <+YOU OR YOUR COMPANY+>.
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
#include "frame_buffer_impl.h"
#include <string.h>
#include <boost/crc.hpp>

// ARP table path
#define ARP_CACHE "/proc/net/arp"

namespace gr {
	namespace macprotocols {

		frame_buffer::sptr
		frame_buffer::make(int buff_size, bool arp) {
			return gnuradio::get_initial_sptr
				(new frame_buffer_impl(buff_size, arp));
		}

		frame_buffer_impl::frame_buffer_impl(int buff_size, bool arp)
			: gr::block("frame_buffer",
							gr::io_signature::make(0, 0, 0),
							gr::io_signature::make(0, 0, 0)),
							buff_size(buff_size), arp(arp) {
			circ_buff.rset_capacity(buff_size);
			std::cout << "Buffer capacity = " << circ_buff.capacity() << std::endl << std::flush;

			message_port_register_in(pmt::mp("frame in"));
			set_msg_handler(pmt::mp("frame in"), boost::bind(&frame_buffer_impl::frame_in, this, _1));

			message_port_register_in(pmt::mp("ctrl in"));
			set_msg_handler(pmt::mp("ctrl in"), boost::bind(&frame_buffer_impl::ctrl_in, this, _1));

			message_port_register_out(pmt::mp("frame out"));
		}

		void frame_buffer_impl::frame_in(pmt::pmt_t frame) {
			if(arp) {
				uint8_t ip_dst[4], mac[6];
				pmt::pmt_t cdr = pmt::cdr(frame);
				uint8_t *f = (uint8_t*) pmt::blob_data(cdr);
				int frame_len = pmt::blob_length(cdr);
				memcpy(ip_dst, f + 48, 4);

				if(lookup_arp(ip_dst, mac)) {
					std::cout << "Dest MAC = ";
					for(int i = 0; i < 5; i++) std::cout << +mac[i] << ":";
					std::cout << +mac[5] << std::endl << std::flush;

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
					std::cout << "Unknown dest MAC" << std::endl << std::flush;
				}
			}

			if(circ_buff.size() == buff_size)
				std::cout << "BUFFER IS FULL!" << std::endl << std::flush;

			circ_buff.push_back(frame);
		}

		bool frame_buffer_impl::lookup_arp(uint8_t *ip, uint8_t *mac) {
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

		void frame_buffer_impl::ctrl_in(pmt::pmt_t msg) {
			// TODO: handle msg according to content
			std::string str = pmt::symbol_to_string(msg);

			if(str == "get frame") {
				if(circ_buff.size() > 0) {
					message_port_pub(pmt::mp("frame out"), circ_buff[0]);
					circ_buff.pop_front();
					std::cout << "Frame was sent from BUFFER. Buffer size = " << circ_buff.size() << std::endl << std::flush;
				}
			}
		}

		frame_buffer_impl::~frame_buffer_impl() {}

		void frame_buffer_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required) {}

		int
		frame_buffer_impl::general_work (int noutput_items,
											 gr_vector_int &ninput_items,
											 gr_vector_const_void_star &input_items,
											 gr_vector_void_star &output_items) {
			consume_each (noutput_items);
			return noutput_items;
		}
	} /* namespace macprotocols */
} /* namespace gr */

