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

#include <chrono>
#include <gnuradio/io_signature.h>
#include "cs_impl.h"
#include <math.h>
#include <pmt/pmt.h>
#include <string>
#include <iostream>
#include <stdlib.h>

#define BUFF 8

namespace gr {
	namespace macprotocols {

	typedef std::chrono::high_resolution_clock clock;

	float threshold; // Power threshold for a busy channel
	pmt::pmt_t out_msg; // Result of medium sensing
	auto start_time = clock::now(); // Tells when sesing has started
	float stime; // Sensing time
	float sample_duration;
	bool sense;
	int count;

		cs::sptr
		cs::make(int num_samples, int gain) {
			return gnuradio::get_initial_sptr
				(new cs_impl(num_samples, gain));
		}

		/*
		 * The private constructor
		 */
		cs_impl::cs_impl(int num_samples, int gain)
			: gr::block("my_csense",
							gr::io_signature::make(1, 1, sizeof(gr_complex)),
							gr::io_signature::make(0, 0, 0)),
				l_gain(gain),
				l_num_samples(num_samples) {
			message_port_register_in(pmt::mp("in_control"));
			set_msg_handler(pmt::mp("in_control"), boost::bind(&cs_impl::in_control, this, _1));

			message_port_register_out(pmt::mp("out_csense"));

			sense = false;
		}

	/*
		 * Our virtual destructor.
		 */
		cs_impl::~cs_impl() {
		}

		void cs_impl::in_control(pmt::pmt_t msg) {
			if(sense == false) {
				// Parsing input. Msg format: "threshold=XXX,time=XXX".
				std::string str = pmt::symbol_to_string(msg);
				int comma = str.find(",");
				std::string temp = str.substr(10, comma - 10);
				threshold = atof(temp.c_str());
				temp = str.substr(comma + 6, str.length());

				// The next lines trigger the sensing
				stime = atof(temp.c_str());
				sample_duration = 0;
				sense = true; // activates sesing phase
				out_msg = pmt::intern("iddle");
				start_time = clock::now();
				count = 0;
			}
		}

		void
		cs_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required) {
		}

		int
		cs_impl::general_work (int noutput_items,
											 gr_vector_int &ninput_items,
											 gr_vector_const_void_star &input_items,
											 gr_vector_void_star &output_items) {
			const gr_complex *in = (const gr_complex *) input_items[0];
			int ninputs = ninput_items.size();
			float power; float max_power = -10000;

			if(sense == true) {// Sensing is activated
				auto end_time = clock::now();
				float duration = (float) std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

				for(int i = 0; i < noutput_items; ) {
					auto sample_start_time = clock::now();

					int j = 0;
					while(j < BUFF and i < noutput_items) {
						power = 20*log10( sqrt(pow((float)in[i].imag(), 2) + pow((float)in[i].real(), 2)) / l_num_samples ) + 10 - l_gain;
						if(power >= max_power)
							max_power = power;
						i++;
						j++;
						count++;
					}

					end_time = clock::now();
					duration = (float) std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
					sample_duration = (float) std::chrono::duration_cast<std::chrono::microseconds>(end_time - sample_start_time).count();

					if(max_power >= threshold)
						out_msg = pmt::intern("busy");
					if((stime - duration <= sample_duration) or (duration >= stime))
						break;
				}

				if ((stime - duration <= sample_duration) or (duration >= stime)) {
					sense = false;
					message_port_pub(pmt::mp("out_csense"), out_msg);
					std::cout << "duration = " << duration << ", medium1 = " << out_msg << ", max power = " << max_power << ", threshold = " << threshold << ", count = " << count << std::endl;
				}
				
			}

			// Do <+signal processing+>
			// Tell runtime system how many input items we consumed on
			// each input stream.
			consume_each (ninputs);

			// Tell runtime system how many output items we produced.
			return noutput_items;
		}

	} /* namespace my_carriersense */
} /* namespace gr */