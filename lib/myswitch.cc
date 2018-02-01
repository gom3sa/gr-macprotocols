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
#include "myswitch.h"
#include <pmt/pmt.h>

using namespace gr::macprotocols;

class myswitch_impl : public myswitch {
	public:
		myswitch_impl(uint8_t portid)
		: gr::block("myswitch",
			gr::io_signature::make(0, 0, 0),
			gr::io_signature::make(0, 0, 0)),
			pr_portid(portid) {

			// Input msg ports
			message_port_register_in(msg_port_ctrlin);
			set_msg_handler(msg_port_ctrlin, boost::bind(&myswitch_impl::ctrlin, this, _1));

			message_port_register_in(msg_port_in0);
			set_msg_handler(msg_port_in0, boost::bind(&myswitch_impl::in0, this, _1));

			message_port_register_in(msg_port_in1);
			set_msg_handler(msg_port_in1, boost::bind(&myswitch_impl::in1, this, _1));

			message_port_register_in(msg_port_in2);
			set_msg_handler(msg_port_in2, boost::bind(&myswitch_impl::in2, this, _1));

			message_port_register_in(msg_port_in3);
			set_msg_handler(msg_port_in3, boost::bind(&myswitch_impl::in3, this, _1));

			message_port_register_in(msg_port_in4);
			set_msg_handler(msg_port_in4, boost::bind(&myswitch_impl::in4, this, _1));

			// Output msg ports
			message_port_register_out(msg_port_out0);
			message_port_register_out(msg_port_out1);
			message_port_register_out(msg_port_out2);
			message_port_register_out(msg_port_out3);
			message_port_register_out(msg_port_out4);
		}

		void ctrlin(pmt::pmt_t ctrl_msg) {
			std::string str = pmt::symbol_to_string(ctrl_msg);
			
			if(str == "portid-1") pr_portid = -1; // This means there is no chosen protocol
			else if(str == "portid0") pr_portid = 0;
			else if(str == "portid1") pr_portid = 1;
			else if(str == "portid2") pr_portid = 2;
			else if(str == "portid3") pr_portid = 3;
			else if(str == "portid4") pr_portid = 4;
			else return;
		}

		void in0(pmt::pmt_t msg) {
			if(pr_portid == 0) message_port_pub(msg_port_out0, msg);
		}

		void in1(pmt::pmt_t msg) {
			if(pr_portid == 1) message_port_pub(msg_port_out1, msg);
		}

		void in2(pmt::pmt_t msg) {
			if(pr_portid == 2) message_port_pub(msg_port_out2, msg);
		}

		void in3(pmt::pmt_t msg) {
			if(pr_portid == 3) message_port_pub(msg_port_out3, msg);
		}

		void in4(pmt::pmt_t msg) {
			if(pr_portid == 4) message_port_pub(msg_port_out4, msg);
		}

	private:
		uint8_t pr_portid;

		// Input msg ports
		pmt::pmt_t msg_port_ctrlin = pmt::mp("ctrl in");
		pmt::pmt_t msg_port_in0 = pmt::mp("in0");
		pmt::pmt_t msg_port_in1 = pmt::mp("in1");
		pmt::pmt_t msg_port_in2 = pmt::mp("in2");
		pmt::pmt_t msg_port_in3 = pmt::mp("in3");
		pmt::pmt_t msg_port_in4 = pmt::mp("in4");

		// Output msg ports
		pmt::pmt_t msg_port_out0 = pmt::mp("out0");
		pmt::pmt_t msg_port_out1 = pmt::mp("out1");
		pmt::pmt_t msg_port_out2 = pmt::mp("out2");
		pmt::pmt_t msg_port_out3 = pmt::mp("out3");
		pmt::pmt_t msg_port_out4 = pmt::mp("out4");
};

myswitch::sptr
myswitch::make(uint8_t portid) {
	return gnuradio::get_initial_sptr(new myswitch_impl(portid));
}