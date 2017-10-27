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
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "csma_ca_impl.h"
#include <pmt/pmt.h>

namespace gr {
  namespace macprotocols {

    csma_ca::sptr
    csma_ca::make(int slot_time, int sifs, int difs)
    {
      return gnuradio::get_initial_sptr
        (new csma_ca_impl(slot_time, sifs, difs));
    }

    /*
     * The private constructor
     */
    csma_ca_impl::csma_ca_impl(int slot_time, int sifs, int difs)
      : gr::block("csma_ca",
              gr::io_signature::make(0, 0, 0),
              gr::io_signature::make(0, 0, 0)),
              l_slot_time(slot_time), l_sifs(sifs), l_difs(difs)
    {
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
      message_port_pub(pmt::mp("phy out"), frame);
    }

    void csma_ca_impl::phy_in(pmt::pmt_t frame)
    {
      message_port_pub(pmt::mp("mac out"), frame);
    }

    /*
     * Where the magics ends
    /*

    /*
     * Our virtual destructor.
     */
    csma_ca_impl::~csma_ca_impl() {}

    void csma_ca_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required) {}

    int csma_ca_impl::general_work (int noutput_items, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
    {
      consume_each (noutput_items);
      return noutput_items;
    }

  } /* namespace macprotocols */
} /* namespace gr */