/* -*- c++ -*- */

#define MACPROTOCOLS_API

%include "gnuradio.i"			// the common stuff

//load generated python docstrings
%include "macprotocols_swig_doc.i"

%{
#include "macprotocols/csma_ca.h"
#include "macprotocols/cs.h"
#include "macprotocols/ack_gen.h"
#include "macprotocols/frame_buffer.h"
%}


%include "macprotocols/csma_ca.h"
GR_SWIG_BLOCK_MAGIC2(macprotocols, csma_ca);
%include "macprotocols/cs.h"
GR_SWIG_BLOCK_MAGIC2(macprotocols, cs);
%include "macprotocols/ack_gen.h"
GR_SWIG_BLOCK_MAGIC2(macprotocols, ack_gen);
%include "macprotocols/frame_buffer.h"
GR_SWIG_BLOCK_MAGIC2(macprotocols, frame_buffer);
