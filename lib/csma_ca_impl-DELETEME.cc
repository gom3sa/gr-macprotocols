/* -*- c++ -*- */
/* 
 * Copyright 2017 , André Gomes <andre.gomes@dcc.ufmg.br>.
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
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
#include "csma_ca_impl.h"
#include <pmt/pmt.h>
#include <boost/circular_buffer.hpp>
#include <chrono>

#define BUFF_SIZE 64
#define TIMEOUT 10 // The right formular for timeout is as follow: timeout = Sifs + aSlotTime + aRxPHYStartDelay
#define MAXRETRIES 5
#define THRESHOLD -70

namespace gr {
	namespace macprotocols {

		typedef std::chrono::high_resolution_clock clock;

		struct buffer {
			pmt::pmt_t frame;
			int frame_id;
			clock::time_point tod;
			int attempts;
			bool acked;
		};

		boost::circular_buffer<struct buffer> cb(BUFF_SIZE);
		int frame_id;

		/*
		 * The private constructor
		 */
		csma_ca_impl::csma_ca_impl(int slot_time, int sifs, int difs)
			: gr::block("csma_ca",
							gr::io_signature::make(0, 0, 0),
							gr::io_signature::make(0, 0, 0)),
							l_slot_time(slot_time), l_sifs(sifs), l_difs(difs)
		{
			// Variables
			frame_id = 0;

			// Inputs
			message_port_register_in(pmt::mp("mac in"));
			set_msg_handler(pmt::mp("mac in"), boost::bind(&csma_ca_impl::mac_in, this, _1));

			message_port_register_in(pmt::mp("phy in"));
			set_msg_handler(pmt::mp("phy in"), boost::bind(&csma_ca_impl::phy_in, this, _1));

			// Outputs
			message_port_register_out(pmt::mp("phy out"));

			message_port_register_out(pmt::mp("mac out"));
		}

		/*
		 * Where the magic happens, baby
		*/

		void csma_ca_impl::mac_in(pmt::pmt_t frame)
		{
			if(frame_id > BUFF_SIZE) // If true, renew frame ids
				frame_id = 0;

			struct buffer b;
			b.frame_id = frame_id; frame_id++;
			b.frame = frame;
			b.attempts = 0;
			b.acked = false;

			cb.push_back(b);

			//if(cb.size() == 1)
				start_protocol();

			std::cout << "new frame has arrived" << std::endl;
		}

		void csma_ca_impl::phy_in(pmt::pmt_t frame)
		{
			message_port_pub(pmt::mp("mac out"), frame);
		}

		void csma_ca_impl::start_protocol()
		{
			//while(cb.size() > 0)
			//{
				std::cout << "Frames to be sent = " << cb.size() << std::endl;

				/*
				* Checks queue for either retransmission or fresh frames
				*/
				clock::time_point now;
				float duration;
				for(int i = 0; i < cb.size(); i++)
				{
					if(cb[i].attempts > 0) // Check for retransmissions
					{
						now = clock::now();
						duration = (float) std::chrono::duration_cast<std::chrono::microseconds>(now - cb[i].tod).count();
						if(duration >= TIMEOUT and cb[i].attempts < MAXRETRIES) // Try retransmit again
						{
							std::cout << "Frame " << i << " must be retransmitted" << std::endl;
							cb[i].tod = clock::now();
							cb[i].attempts++;
							message_port_pub(pmt::mp("phy out"), cb[i].frame);
						}
						else if(duration >= TIMEOUT and cb[i].attempts == MAXRETRIES) // Retransmission is over. Frame will be dropped by last inner while loop
						{
							std::cout << "Frame " << i << " will be dropped" << std::endl;
							cb[i].attempts++;
						}				
					}
					else if(cb[i].attempts == 0) // Fresh frames on the queue
					{
						std::cout << "Frame " << i << " will be sent now" << std::endl;
						cb[i].tod = clock::now();
						cb[i].attempts++;
						message_port_pub(pmt::mp("phy out"), cb[i].frame);
					}
				//}

				std::cout << "Queue size = " << cb.size() << std::endl;

				/*
				* Shrink queue in case of max retries or acked frames
				WARNING: The circular buffer size should be big enough to avoid overflow before timeouts!
				*/
				while(cb[0].attempts > MAXRETRIES or cb[0].acked == true)
				//	cb.pop_front();

			}
		}
		
		/*
		 * Where the magics ends
		/*

		/*
		 * Our virtual destructor.
		 */
		csma_ca::sptr
		csma_ca::make(int slot_time, int sifs, int difs)
		{
			return gnuradio::get_initial_sptr
				(new csma_ca_impl(slot_time, sifs, difs));
		}

		csma_ca_impl::~csma_ca_impl() {}

		void csma_ca_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required) {}

		int csma_ca_impl::general_work (int noutput_items, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
		{
			std::cout<< "hello" << std::endl;
			consume_each (noutput_items);	
			return noutput_items;
		}


	} /* namespace macprotocols */
} /* namespace gr */