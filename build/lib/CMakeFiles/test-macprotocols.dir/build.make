# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.7

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/andre/gnuradio/gr-macprotocols

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/andre/gnuradio/gr-macprotocols/build

# Include any dependencies generated for this target.
include lib/CMakeFiles/test-macprotocols.dir/depend.make

# Include the progress variables for this target.
include lib/CMakeFiles/test-macprotocols.dir/progress.make

# Include the compile flags for this target's objects.
include lib/CMakeFiles/test-macprotocols.dir/flags.make

lib/CMakeFiles/test-macprotocols.dir/test_macprotocols.cc.o: lib/CMakeFiles/test-macprotocols.dir/flags.make
lib/CMakeFiles/test-macprotocols.dir/test_macprotocols.cc.o: ../lib/test_macprotocols.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/andre/gnuradio/gr-macprotocols/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object lib/CMakeFiles/test-macprotocols.dir/test_macprotocols.cc.o"
	cd /home/andre/gnuradio/gr-macprotocols/build/lib && /usr/bin/c++   $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/test-macprotocols.dir/test_macprotocols.cc.o -c /home/andre/gnuradio/gr-macprotocols/lib/test_macprotocols.cc

lib/CMakeFiles/test-macprotocols.dir/test_macprotocols.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/test-macprotocols.dir/test_macprotocols.cc.i"
	cd /home/andre/gnuradio/gr-macprotocols/build/lib && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/andre/gnuradio/gr-macprotocols/lib/test_macprotocols.cc > CMakeFiles/test-macprotocols.dir/test_macprotocols.cc.i

lib/CMakeFiles/test-macprotocols.dir/test_macprotocols.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/test-macprotocols.dir/test_macprotocols.cc.s"
	cd /home/andre/gnuradio/gr-macprotocols/build/lib && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/andre/gnuradio/gr-macprotocols/lib/test_macprotocols.cc -o CMakeFiles/test-macprotocols.dir/test_macprotocols.cc.s

lib/CMakeFiles/test-macprotocols.dir/test_macprotocols.cc.o.requires:

.PHONY : lib/CMakeFiles/test-macprotocols.dir/test_macprotocols.cc.o.requires

lib/CMakeFiles/test-macprotocols.dir/test_macprotocols.cc.o.provides: lib/CMakeFiles/test-macprotocols.dir/test_macprotocols.cc.o.requires
	$(MAKE) -f lib/CMakeFiles/test-macprotocols.dir/build.make lib/CMakeFiles/test-macprotocols.dir/test_macprotocols.cc.o.provides.build
.PHONY : lib/CMakeFiles/test-macprotocols.dir/test_macprotocols.cc.o.provides

lib/CMakeFiles/test-macprotocols.dir/test_macprotocols.cc.o.provides.build: lib/CMakeFiles/test-macprotocols.dir/test_macprotocols.cc.o


lib/CMakeFiles/test-macprotocols.dir/qa_macprotocols.cc.o: lib/CMakeFiles/test-macprotocols.dir/flags.make
lib/CMakeFiles/test-macprotocols.dir/qa_macprotocols.cc.o: ../lib/qa_macprotocols.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/andre/gnuradio/gr-macprotocols/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object lib/CMakeFiles/test-macprotocols.dir/qa_macprotocols.cc.o"
	cd /home/andre/gnuradio/gr-macprotocols/build/lib && /usr/bin/c++   $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/test-macprotocols.dir/qa_macprotocols.cc.o -c /home/andre/gnuradio/gr-macprotocols/lib/qa_macprotocols.cc

lib/CMakeFiles/test-macprotocols.dir/qa_macprotocols.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/test-macprotocols.dir/qa_macprotocols.cc.i"
	cd /home/andre/gnuradio/gr-macprotocols/build/lib && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/andre/gnuradio/gr-macprotocols/lib/qa_macprotocols.cc > CMakeFiles/test-macprotocols.dir/qa_macprotocols.cc.i

lib/CMakeFiles/test-macprotocols.dir/qa_macprotocols.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/test-macprotocols.dir/qa_macprotocols.cc.s"
	cd /home/andre/gnuradio/gr-macprotocols/build/lib && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/andre/gnuradio/gr-macprotocols/lib/qa_macprotocols.cc -o CMakeFiles/test-macprotocols.dir/qa_macprotocols.cc.s

lib/CMakeFiles/test-macprotocols.dir/qa_macprotocols.cc.o.requires:

.PHONY : lib/CMakeFiles/test-macprotocols.dir/qa_macprotocols.cc.o.requires

lib/CMakeFiles/test-macprotocols.dir/qa_macprotocols.cc.o.provides: lib/CMakeFiles/test-macprotocols.dir/qa_macprotocols.cc.o.requires
	$(MAKE) -f lib/CMakeFiles/test-macprotocols.dir/build.make lib/CMakeFiles/test-macprotocols.dir/qa_macprotocols.cc.o.provides.build
.PHONY : lib/CMakeFiles/test-macprotocols.dir/qa_macprotocols.cc.o.provides

lib/CMakeFiles/test-macprotocols.dir/qa_macprotocols.cc.o.provides.build: lib/CMakeFiles/test-macprotocols.dir/qa_macprotocols.cc.o


# Object files for target test-macprotocols
test__macprotocols_OBJECTS = \
"CMakeFiles/test-macprotocols.dir/test_macprotocols.cc.o" \
"CMakeFiles/test-macprotocols.dir/qa_macprotocols.cc.o"

# External object files for target test-macprotocols
test__macprotocols_EXTERNAL_OBJECTS =

lib/test-macprotocols: lib/CMakeFiles/test-macprotocols.dir/test_macprotocols.cc.o
lib/test-macprotocols: lib/CMakeFiles/test-macprotocols.dir/qa_macprotocols.cc.o
lib/test-macprotocols: lib/CMakeFiles/test-macprotocols.dir/build.make
lib/test-macprotocols: /usr/lib/x86_64-linux-gnu/libgnuradio-runtime.so
lib/test-macprotocols: /usr/lib/x86_64-linux-gnu/libgnuradio-pmt.so
lib/test-macprotocols: /usr/lib/x86_64-linux-gnu/libboost_filesystem.so
lib/test-macprotocols: /usr/lib/x86_64-linux-gnu/libboost_system.so
lib/test-macprotocols: /usr/lib/x86_64-linux-gnu/libcppunit.so
lib/test-macprotocols: lib/libgnuradio-macprotocols.so
lib/test-macprotocols: /usr/lib/x86_64-linux-gnu/libgnuradio-runtime.so
lib/test-macprotocols: /usr/lib/x86_64-linux-gnu/libgnuradio-pmt.so
lib/test-macprotocols: /usr/lib/x86_64-linux-gnu/libboost_filesystem.so
lib/test-macprotocols: /usr/lib/x86_64-linux-gnu/libboost_system.so
lib/test-macprotocols: lib/CMakeFiles/test-macprotocols.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/andre/gnuradio/gr-macprotocols/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking CXX executable test-macprotocols"
	cd /home/andre/gnuradio/gr-macprotocols/build/lib && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/test-macprotocols.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
lib/CMakeFiles/test-macprotocols.dir/build: lib/test-macprotocols

.PHONY : lib/CMakeFiles/test-macprotocols.dir/build

lib/CMakeFiles/test-macprotocols.dir/requires: lib/CMakeFiles/test-macprotocols.dir/test_macprotocols.cc.o.requires
lib/CMakeFiles/test-macprotocols.dir/requires: lib/CMakeFiles/test-macprotocols.dir/qa_macprotocols.cc.o.requires

.PHONY : lib/CMakeFiles/test-macprotocols.dir/requires

lib/CMakeFiles/test-macprotocols.dir/clean:
	cd /home/andre/gnuradio/gr-macprotocols/build/lib && $(CMAKE_COMMAND) -P CMakeFiles/test-macprotocols.dir/cmake_clean.cmake
.PHONY : lib/CMakeFiles/test-macprotocols.dir/clean

lib/CMakeFiles/test-macprotocols.dir/depend:
	cd /home/andre/gnuradio/gr-macprotocols/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/andre/gnuradio/gr-macprotocols /home/andre/gnuradio/gr-macprotocols/lib /home/andre/gnuradio/gr-macprotocols/build /home/andre/gnuradio/gr-macprotocols/build/lib /home/andre/gnuradio/gr-macprotocols/build/lib/CMakeFiles/test-macprotocols.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : lib/CMakeFiles/test-macprotocols.dir/depend

