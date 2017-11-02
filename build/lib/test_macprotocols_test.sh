#!/bin/sh
export VOLK_GENERIC=1
export GR_DONT_LOAD_PREFS=1
export srcdir=/home/andre/gnuradio/gr-macprotocols/lib
export GR_CONF_CONTROLPORT_ON=False
export PATH=/home/andre/gnuradio/gr-macprotocols/build/lib:$PATH
export LD_LIBRARY_PATH=/home/andre/gnuradio/gr-macprotocols/build/lib:$LD_LIBRARY_PATH
export PYTHONPATH=$PYTHONPATH
test-macprotocols 
