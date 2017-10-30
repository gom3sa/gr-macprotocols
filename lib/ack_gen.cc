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
#include <macprotocols/ack_gen.h>
#include <pmt/pmt.h>
#include <string>

using namespace gr::macprotocols;

class ack_gen_impl : public ack_gen {
	public:
		ack_gen_impl()
			: gr::block("ack_gen",
							gr::io_signature::make(0, 0, 0),
							gr::io_signature::make(0, 0, 0)) {

				message_port_register_in(pmt::mp("frame in"));
				set_msg_handler(pmt::mp("frame in"), boost::bind(&ack_gen_impl::frame_in, this, _1));

				message_port_register_out(pmt::mp("ack out"));
		}

		void frame_in(pmt::pmt_t frame) {
			std::string str = "ack";
			pmt::pmt_t ack = pmt::intern(str);
			message_port_pub(pmt::mp("ack out"), ack);
		}

	private:

};

ack_gen::sptr
ack_gen::make() {
	return gnuradio::get_initial_sptr
		(new ack_gen_impl());
}