Hi! This is still a work in progress. The first MAC protocols should come up in the next weeks. The expected release order is:

1. CSMA/CA 	(done!)
2. TDMA		(done!)
3. DCF
4. CDMA
5. Not defined yet

## Dependencies

This project depends on features available on [gr-toolkit](https://github.com/andreviniciusgsg/gr-toolkit.git). In order to install gr-toolkit, follow the steps below.

`git clone https://github.com/andreviniciusgsg/gr-toolkit.git` <br />
`cd gr-toolkit` <br />
`mkdir build` <br />
`cd build` <br />
`cmake ..` <br />
`sudo make install` <br />
`sudo ldconfig` <br />

Furthermore, this is a Gnu Radio application. So, I assume Gnu Radio is already installed on you computer. If it is not the case, see [Gnu Radio website](https://wiki.gnuradio.org/index.php/InstallingGR) for more details.

## Installation

This feature uses C++11. If you find any errors refering to C++11 while compiling, check if `set(CMAKE_CXX_STANDARD 11)` is present on file CMakeLists.txt. If everything is okay, the commands below should install this module properly.

`git clone https://github.com/andreviniciusgsg/gr-macprotocols.git` <br />
`cd gr-macprotocols` <br />
`mkdir build` <br />
`cd build` <br />
`cmake ..` <br />
`make` <br />
`sudo make install` <br />
`sudo ldconfig` <br />

## Quick Installation

In order to make life easier, we provide an installation script that installs all dependencies for running gr-macprotocols over gr-ieee802-11 but Gnu Radio. The script is called `install_gr-macprotocols.sh` and is included in this git folder.

## Examples

Examples are provided and can be found in folder `examples`. I suggest you to open files `wifi_transceiver_nogui_*.grc` in Gnu Radio Companion and have a look at it. These are ready-to-go examples of transcievers with MAC protocol CSMA/CA over the PHY layer based on IEEE 802.11. In order to run these examples, **you must install** the project [gr-ieee802.11](https://github.com/bastibl/gr-ieee802-11).

### Tun/ Tap Examples

It is necessary to configure a tap interface in order to run Tun/ Tap examples. A configuration script is given at the folder `apps`. You should execute script `config_interface_tuntap_X.sh` at the same machine you run example `wifi_transceiver_tuntap_X.py`, always matching the id given by X. Check `config_interface_tuntap_X.sh` to get more information about IP and MAC addresses.

## Overflows

If a bunch of Os is generated at screen while running the examples, you are probably experiencing overflows. It means your computer is not able to process the amount of data it has been given by Gnu Radio. Try to increase the `alpha` parameter, it may help. 

## Thoughts on timing

As pointed out by [Bloessl](https://www.researchgate.net/publication/276279753_Timings_Matter_Standard_Compliant_IEEE_80211_Channel_Access_for_a_Fully_Software-based_SDR_Architecture)'s work, timings refrain software implementations of MAC protocols from operating according to IEEE 802.11 standard requirement. The proposed implementation has a parameter called `alpha` that consists of a multiplier of timing parameters. For example, if `SIFS = 10us` and `alpha = 100`, the considered SIFS time becomes `SIFS = 1000us` in respect of CSMA/CA.

## Author and Contact

Author: André Vinícius Gomes Santos Gonçalves <br />
Work address: Winet research group of the Computer Science Department of the Federal University of Minas Gerais

Feel free to reach me. My email address is andre.gomes@dcc.ufmg.br. You can also find more information about myself in [https://homepages.dcc.ufmg.br/~andre.gomes](https://homepages.dcc.ufmg.br/~andre.gomes). 
