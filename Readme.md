Hi! 

This is still a work in progress. The first MAC protocols should come up in the next weeks. The expected release order is:

1. CSMA/CA
2. DCF
3. TDMA
4. CDMA
5. Not defined yet

## Dependencies

This project depends on features available on [gr-toolkit](https://github.com/andreviniciusgsg/gr-toolkit.git). In order to install gr-toolkit, follow the steps below.

`git clone https://github.com/andreviniciusgsg/gr-toolkit.git` <br />
`cd gr-toolkit` <br />
`mkdir build` <br />
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

## Examples

Examples are provided and can be found in folder `examples`. I suggest you to open files `wifi_transceiver_nogui_*.grc` in Gnu Radio Companion and have a look at it. These are ready-to-go examples of transcievers with MAC protocol CSMA/CA over the PHY layer based on IEEE 802.11. In order to run these examples, **you must install** the project [gr-ieee802.11](https://github.com/bastibl/gr-ieee802-11).

## Contact 

Feel free to reach me. My email address is andre.gomes@dcc.ufmg.br. You can also find more information about myself in [https://homepages.dcc.ufmg.br/~andre.gomes](https://homepages.dcc.ufmg.br/~andre.gomes). 
