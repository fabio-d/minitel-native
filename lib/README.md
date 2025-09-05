# Helper libraries for native Minitel programming

## Contents

Collection of helper libraries for writing native Minitel programs.

* `minitel_board`: Contains definitions that are specific to each Minitel model.
* `minitel_keyboard`: Functions to retrieve the state of the keyboard and
  print key names into text form.
* `minitel_timer`: Functions to convert intervals and baud rates into the
  corresponding number of clock cycles.
* `minitel_video`: Constants and memory maps for interacting with the EF9345 and
  TS9347 semi-graphic display processors.

## Usage

In order to use these libraries in a program whose source code is hosted on a
different repository, create a `CMakeLists.txt` file like this:
```cmake
cmake_minimum_required(VERSION 3.13)

include(${MINITEL_NATIVE_PATH}/lib/cmake/init.cmake)

project(my_minitel_program C)
minitel_lib_init()

add_executable(my_minitel_program main.c)
target_link_libraries(my_minitel_program PRIVATE minitel_keyboard minitel_video)
minitel_add_bin_output(my_minitel_program)
```

Then clone this repository separately, and pass its path to `cmake`. For
instance:
```shell
# Download the repository into the home directory.
$ git clone https://github.com/fabio-d/minitel-native ~/minitel-native

# Run this in the same folder as the CMakeLists.txt above.
$ mkdir build
$ cd build
$ cmake .. -DMINITEL_NATIVE_PATH=~/minitel-native -DMINITEL_MODEL=nfz330
$ make
```

See [`cmake/init.cmake`](cmake/init.cmake) for the list of supported models.
