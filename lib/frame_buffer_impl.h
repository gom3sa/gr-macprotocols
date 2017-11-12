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

#ifndef INCLUDED_MACPROTOCOLS_FRAME_BUFFER_IMPL_H
#define INCLUDED_MACPROTOCOLS_FRAME_BUFFER_IMPL_H

#include <macprotocols/frame_buffer.h>
#include <boost/circular_buffer.hpp>
#include <pmt/pmt.h>

namespace gr {
	namespace macprotocols {

		class frame_buffer_impl : public frame_buffer
		{
		 private:
			int buff_size;
			boost::circular_buffer<pmt::pmt_t> circ_buff;

		 public:
			frame_buffer_impl(int buff_size);
			~frame_buffer_impl();

			// Where all the action really happens

			void frame_in(pmt::pmt_t frame);

			void ctrl_in(pmt::pmt_t msg);

			void forecast (int noutput_items, gr_vector_int &ninput_items_required);

			int general_work(int noutput_items,
					 gr_vector_int &ninput_items,
					 gr_vector_const_void_star &input_items,
					 gr_vector_void_star &output_items);
		};

	} // namespace macprotocols
} // namespace gr

#endif /* INCLUDED_MACPROTOCOLS_FRAME_BUFFER_IMPL_H */

