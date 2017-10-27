/* -*- c++ -*- */

#define MACPROTOCOLS_API

%include "gnuradio.i"			// the common stuff

//load generated python docstrings
%include "macprotocols_swig_doc.i"

%{
#include "macprotocols/csma_ca.h"
%}


%include "macprotocols/csma_ca.h"
GR_SWIG_BLOCK_MAGIC2(macprotocols, csma_ca);
