Hi! 

This is still a work in progress. The first MAC protocols should come up in the next weeks. The expected release order is:

1. CSMA/CA
2. DCF
3. TDMA
4. CDMA
5. Not defined yet

# Installation

This feature uses C++11. If you find any errors refering to C++11 while compiling, check if `set(CMAKE_CXX_STANDARD 11)` is present on file CMakeLists.txt. If everything is okay, the commands below should install this module properly.

`git clone https://github.com/andreviniciusgsg/gr-csmaca.git` <br />
`cd gr-csmaca` <br />
`mkdir build` <br />
`cd build` <br />
`cmake ..` <br />
`sudo make install` <br />
`sudo ldconfig` <br />
