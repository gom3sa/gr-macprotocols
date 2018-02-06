#!/usr/bin/env python2
# -*- coding: utf-8 -*-
##################################################
# GNU Radio Python Flow Graph
# Title: Wifi Transceiver Tdma Tuntap Gateway
# Generated: Tue Feb  6 18:54:55 2018
##################################################

import os
import sys
sys.path.append(os.environ.get('GRC_HIER_PATH', os.path.expanduser('~/.grc_gnuradio')))

from gnuradio import blocks
from gnuradio import eng_notation
from gnuradio import gr
from gnuradio import uhd
from gnuradio.eng_option import eng_option
from gnuradio.filter import firdes
from optparse import OptionParser
from wifi_phy_hier import wifi_phy_hier  # grc-generated hier_block
import foo
import ieee802_11
import macprotocols
import time


class wifi_transceiver_TDMA_tuntap_GATEWAY(gr.top_block):

    def __init__(self):
        gr.top_block.__init__(self, "Wifi Transceiver Tdma Tuntap Gateway")

        ##################################################
        # Variables
        ##################################################
        self.tx_gain = tx_gain = 750e-3
        self.samp_rate = samp_rate = 5e6
        self.rx_gain = rx_gain = 500e-3
        self.pdu_length = pdu_length = 500
        self.mac_dst = mac_dst = [0x12,0x34,0x56,0x78,0x90,0xab]
        self.mac_addr = mac_addr = [0x12,0x34,0x56,0x78,0x90,0xaa]
        self.lo_offset = lo_offset = 0
        self.interval = interval = 1e3
        self.freq = freq = 2.52e9
        self.encoding = encoding = 0
        self.chan_est = chan_est = 0

        ##################################################
        # Blocks
        ##################################################
        self.wifi_phy_hier_0 = wifi_phy_hier(
            bandwidth=samp_rate,
            chan_est=chan_est,
            encoding=encoding,
            frequency=freq,
            sensitivity=0.56,
        )
        self.uhd_usrp_source_0 = uhd.usrp_source(
        	",".join(("", "")),
        	uhd.stream_args(
        		cpu_format="fc32",
        		channels=range(1),
        	),
        )
        self.uhd_usrp_source_0.set_time_now(uhd.time_spec(time.time()), uhd.ALL_MBOARDS)
        self.uhd_usrp_source_0.set_samp_rate(samp_rate)
        self.uhd_usrp_source_0.set_center_freq(uhd.tune_request(freq, rf_freq = freq - lo_offset, rf_freq_policy=uhd.tune_request.POLICY_MANUAL), 0)
        self.uhd_usrp_source_0.set_normalized_gain(rx_gain, 0)
        self.uhd_usrp_sink_0_0 = uhd.usrp_sink(
        	",".join(("", "")),
        	uhd.stream_args(
        		cpu_format="fc32",
        		channels=range(1),
        	),
        	"packet_len",
        )
        self.uhd_usrp_sink_0_0.set_time_now(uhd.time_spec(time.time()), uhd.ALL_MBOARDS)
        self.uhd_usrp_sink_0_0.set_samp_rate(samp_rate)
        self.uhd_usrp_sink_0_0.set_center_freq(uhd.tune_request(freq, rf_freq = freq - lo_offset, rf_freq_policy=uhd.tune_request.POLICY_MANUAL), 0)
        self.uhd_usrp_sink_0_0.set_normalized_gain(tx_gain, 0)
        self.macprotocols_tdma_0 = macprotocols.tdma(True, (mac_addr), 9, 1000, True)
        self.macprotocols_frame_buffer_0 = macprotocols.frame_buffer(256, True, 0, True)
        self.ieee802_11_parse_mac_0 = ieee802_11.parse_mac(False, False)
        self.ieee802_11_mac_0_0 = ieee802_11.mac((mac_addr), (mac_dst), ([0xff, 0xff, 0xff, 0xff, 0xff, 255]))
        (self.ieee802_11_mac_0_0).set_min_output_buffer(256)
        (self.ieee802_11_mac_0_0).set_max_output_buffer(4096)
        self.ieee802_11_ether_encap_0 = ieee802_11.ether_encap(True)
        self.foo_wireshark_connector_0_0 = foo.wireshark_connector(127, True)
        self.foo_wireshark_connector_0 = foo.wireshark_connector(127, True)
        self.foo_packet_pad2_0 = foo.packet_pad2(False, False, 0.001, 10000, 10000)
        (self.foo_packet_pad2_0).set_min_output_buffer(100000)
        self.blocks_tuntap_pdu_0 = blocks.tuntap_pdu("tap0", 440, False)
        self.blocks_multiply_const_vxx_0 = blocks.multiply_const_vcc((0.6, ))
        (self.blocks_multiply_const_vxx_0).set_min_output_buffer(100000)
        self.blocks_file_sink_0_0 = blocks.file_sink(gr.sizeof_char*1, "/tmp/wifi_rx.pcap", False)
        self.blocks_file_sink_0_0.set_unbuffered(True)
        self.blocks_file_sink_0 = blocks.file_sink(gr.sizeof_char*1, "/tmp/wifi_rx_all.pcap", False)
        self.blocks_file_sink_0.set_unbuffered(True)

        ##################################################
        # Connections
        ##################################################
        self.msg_connect((self.blocks_tuntap_pdu_0, 'pdus'), (self.ieee802_11_ether_encap_0, 'from tap'))    
        self.msg_connect((self.ieee802_11_ether_encap_0, 'to tap'), (self.blocks_tuntap_pdu_0, 'pdus'))    
        self.msg_connect((self.ieee802_11_ether_encap_0, 'to wifi'), (self.ieee802_11_mac_0_0, 'app in'))    
        self.msg_connect((self.ieee802_11_mac_0_0, 'phy out'), (self.macprotocols_frame_buffer_0, 'app in'))    
        self.msg_connect((self.macprotocols_frame_buffer_0, 'frame out 0'), (self.macprotocols_tdma_0, 'frame from buffer'))    
        self.msg_connect((self.macprotocols_tdma_0, 'frame to app'), (self.foo_wireshark_connector_0_0, 'in'))    
        self.msg_connect((self.macprotocols_tdma_0, 'frame to app'), (self.ieee802_11_ether_encap_0, 'from wifi'))    
        self.msg_connect((self.macprotocols_tdma_0, 'frame request'), (self.macprotocols_frame_buffer_0, 'req in 0'))    
        self.msg_connect((self.macprotocols_tdma_0, 'frame to phy'), (self.wifi_phy_hier_0, 'mac_in'))    
        self.msg_connect((self.wifi_phy_hier_0, 'mac_out'), (self.foo_wireshark_connector_0, 'in'))    
        self.msg_connect((self.wifi_phy_hier_0, 'mac_out'), (self.ieee802_11_parse_mac_0, 'in'))    
        self.msg_connect((self.wifi_phy_hier_0, 'mac_out'), (self.macprotocols_tdma_0, 'frame from phy'))    
        self.connect((self.blocks_multiply_const_vxx_0, 0), (self.foo_packet_pad2_0, 0))    
        self.connect((self.foo_packet_pad2_0, 0), (self.uhd_usrp_sink_0_0, 0))    
        self.connect((self.foo_wireshark_connector_0, 0), (self.blocks_file_sink_0, 0))    
        self.connect((self.foo_wireshark_connector_0_0, 0), (self.blocks_file_sink_0_0, 0))    
        self.connect((self.uhd_usrp_source_0, 0), (self.wifi_phy_hier_0, 0))    
        self.connect((self.wifi_phy_hier_0, 0), (self.blocks_multiply_const_vxx_0, 0))    

    def get_tx_gain(self):
        return self.tx_gain

    def set_tx_gain(self, tx_gain):
        self.tx_gain = tx_gain
        self.uhd_usrp_sink_0_0.set_normalized_gain(self.tx_gain, 0)
        	

    def get_samp_rate(self):
        return self.samp_rate

    def set_samp_rate(self, samp_rate):
        self.samp_rate = samp_rate
        self.uhd_usrp_sink_0_0.set_samp_rate(self.samp_rate)
        self.uhd_usrp_source_0.set_samp_rate(self.samp_rate)
        self.wifi_phy_hier_0.set_bandwidth(self.samp_rate)

    def get_rx_gain(self):
        return self.rx_gain

    def set_rx_gain(self, rx_gain):
        self.rx_gain = rx_gain
        self.uhd_usrp_source_0.set_normalized_gain(self.rx_gain, 0)
        	

    def get_pdu_length(self):
        return self.pdu_length

    def set_pdu_length(self, pdu_length):
        self.pdu_length = pdu_length

    def get_mac_dst(self):
        return self.mac_dst

    def set_mac_dst(self, mac_dst):
        self.mac_dst = mac_dst

    def get_mac_addr(self):
        return self.mac_addr

    def set_mac_addr(self, mac_addr):
        self.mac_addr = mac_addr

    def get_lo_offset(self):
        return self.lo_offset

    def set_lo_offset(self, lo_offset):
        self.lo_offset = lo_offset
        self.uhd_usrp_sink_0_0.set_center_freq(uhd.tune_request(self.freq, rf_freq = self.freq - self.lo_offset, rf_freq_policy=uhd.tune_request.POLICY_MANUAL), 0)
        self.uhd_usrp_source_0.set_center_freq(uhd.tune_request(self.freq, rf_freq = self.freq - self.lo_offset, rf_freq_policy=uhd.tune_request.POLICY_MANUAL), 0)

    def get_interval(self):
        return self.interval

    def set_interval(self, interval):
        self.interval = interval

    def get_freq(self):
        return self.freq

    def set_freq(self, freq):
        self.freq = freq
        self.uhd_usrp_sink_0_0.set_center_freq(uhd.tune_request(self.freq, rf_freq = self.freq - self.lo_offset, rf_freq_policy=uhd.tune_request.POLICY_MANUAL), 0)
        self.uhd_usrp_source_0.set_center_freq(uhd.tune_request(self.freq, rf_freq = self.freq - self.lo_offset, rf_freq_policy=uhd.tune_request.POLICY_MANUAL), 0)
        self.wifi_phy_hier_0.set_frequency(self.freq)

    def get_encoding(self):
        return self.encoding

    def set_encoding(self, encoding):
        self.encoding = encoding
        self.wifi_phy_hier_0.set_encoding(self.encoding)

    def get_chan_est(self):
        return self.chan_est

    def set_chan_est(self, chan_est):
        self.chan_est = chan_est
        self.wifi_phy_hier_0.set_chan_est(self.chan_est)


def main(top_block_cls=wifi_transceiver_TDMA_tuntap_GATEWAY, options=None):
    if gr.enable_realtime_scheduling() != gr.RT_OK:
        print "Error: failed to enable real-time scheduling."

    tb = top_block_cls()
    tb.start()
    try:
        raw_input('Press Enter to quit: ')
    except EOFError:
        pass
    tb.stop()
    tb.wait()


if __name__ == '__main__':
    main()
