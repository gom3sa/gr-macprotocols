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

namespace gr {
	namespace macprotocols {

		frame_buffer::sptr
		frame_buffer::make(int buff_size)
		{
			return gnuradio::get_initial_sptr
				(new frame_buffer_impl(buff_size));
		}

		/*
		 * The private constructor
		 */
		frame_buffer_impl::frame_buffer_impl(int buff_size)
			: gr::block("frame_buffer",
							gr::io_signature::make(0, 0, 0),
							gr::io_signature::make(0, 0, 0)),
							buff_size(buff_size)
		{
			circ_buff.rset_capacity(buff_size);
			std::cout << "Buffer capacity = " << circ_buff.capacity() << std::endl << std::flush;

			message_port_register_in(pmt::mp("frame in"));
			set_msg_handler(pmt::mp("frame in"), boost::bind(&frame_buffer_impl::frame_in, this, _1));

			message_port_register_in(pmt::mp("ctrl in"));
			set_msg_handler(pmt::mp("ctrl in"), boost::bind(&frame_buffer_impl::ctrl_in, this, _1));

			message_port_register_out(pmt::mp("frame out"));
		}

		/* 
		 * WHERE THE MAGIC STARTS
		*/
		void frame_buffer_impl::frame_in(pmt::pmt_t frame)
		{
			if(circ_buff.size() == buff_size)
				std::cout << "BUFFER IS FULL!" << std::endl << std::flush;
			circ_buff.push_back(frame);
			//std::cout << "A new frame arrived. Buffer size = " << circ_buff.size() << std::endl;
		}

		void frame_buffer_impl::ctrl_in(pmt::pmt_t msg)
		{
			// TODO: handle msg according to content
			std::string str = pmt::symbol_to_string(msg);

			if(str == "get frame")
			{
				if(circ_buff.size() > 0)
				{
					message_port_pub(pmt::mp("frame out"), circ_buff[0]);
					circ_buff.pop_front();
					std::cout << "Frame was sent from BUFFER. Buffer size = " << circ_buff.size() << std::endl << std::flush;
				}
			}
		}
		/*
		 * WHERE THE MAGIC ENDS
		*/

		/*
		 * Our virtual destructor.
		 */
		frame_buffer_impl::~frame_buffer_impl()
		{
		}

		void
		frame_buffer_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required)
		{
			/* <+forecast+> e.g. ninput_items_required[0] = noutput_items */
		}

		int
		frame_buffer_impl::general_work (int noutput_items,
											 gr_vector_int &ninput_items,
											 gr_vector_const_void_star &input_items,
											 gr_vector_void_star &output_items)
		{
			// Do <+signal processing+>
			// Tell runtime system how many input items we consumed on
			// each input stream.
			consume_each (noutput_items);

			// Tell runtime system how many output items we produced.
			return noutput_items;
		}

	} /* namespace macprotocols */
} /* namespace gr */

