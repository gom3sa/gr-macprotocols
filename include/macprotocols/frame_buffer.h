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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */


#ifndef INCLUDED_MACPROTOCOLS_FRAME_BUFFER_H
#define INCLUDED_MACPROTOCOLS_FRAME_BUFFER_H

#include <macprotocols/api.h>
#include <gnuradio/block.h>

namespace gr {
  namespace macprotocols {

    /*!
     * \brief <+description of block+>
     * \ingroup macprotocols
     *
     */
    class MACPROTOCOLS_API frame_buffer : virtual public gr::block
    {
     public:
      typedef boost::shared_ptr<frame_buffer> sptr;

      /*!
       * \brief Return a shared_ptr to a new instance of macprotocols::frame_buffer.
       *
       * To avoid accidental use of raw pointers, macprotocols::frame_buffer's
       * constructor is in a private implementation
       * class. macprotocols::frame_buffer::make is the public interface for
       * creating new instances.
       */
      static sptr make(int buff_size, bool arp, uint8_t portid, bool debug);
    };

  } // namespace macprotocols
} // namespace gr

#endif /* INCLUDED_MACPROTOCOLS_FRAME_BUFFER_H */

