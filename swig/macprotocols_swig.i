/* -*- c++ -*- */

#define MACPROTOCOLS_API

%include "gnuradio.i"			// the common stuff

//load generated python docstrings
%include "macprotocols_swig_doc.i"

%{
#include "macprotocols/csma_ca.h"
#include "macprotocols/frame_buffer.h"
#include "macprotocols/tdma.h"
#include "macprotocols/myswitch.h"
#include "macprotocols/naive_tdma.h"
%}


%include "macprotocols/csma_ca.h"
GR_SWIG_BLOCK_MAGIC2(macprotocols, csma_ca);
%include "macprotocols/frame_buffer.h"
GR_SWIG_BLOCK_MAGIC2(macprotocols, frame_buffer);
%include "macprotocols/tdma.h"
GR_SWIG_BLOCK_MAGIC2(macprotocols, tdma);
%include "macprotocols/myswitch.h"
GR_SWIG_BLOCK_MAGIC2(macprotocols, myswitch);
%include "macprotocols/naive_tdma.h"
GR_SWIG_BLOCK_MAGIC2(macprotocols, naive_tdma);
