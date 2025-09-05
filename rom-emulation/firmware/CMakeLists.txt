cmake_minimum_required(VERSION 3.13)

# Select the Raspberry Pico model.
set(PICO_BOARD "" CACHE STRING "Target Raspberry Pico model")
if(NOT (PICO_BOARD STREQUAL pico2 OR PICO_BOARD STREQUAL pico2_w))
  message(FATAL_ERROR "Please set cmake ... -DPICO_BOARD=...")
endif()

# Initialize the Pico SDK.
set(PICO_SDK_PATH "" CACHE PATH "Path to Raspberry Pico SDK")
if(NOT PICO_SDK_PATH)
  message(FATAL_ERROR "Please set cmake ... -DPICO_SDK_PATH=...")
endif()
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

include(ExternalProject)

project(rom-emulator ASM C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

find_package(Python3 COMPONENTS Interpreter)

pico_sdk_init()

# Select the Minitel model and the pinout of our connections to the bus.
set(MINITEL_MODEL "" CACHE STRING "Minitel model that will host the ROM emulator")
if(NOT MINITEL_MODEL)
  message(FATAL_ERROR "Please set cmake ... -DMINITEL_MODEL=...")
elseif(MINITEL_MODEL STREQUAL "nfz330")
  set(PINOUT "A8,A9,A10,A11,A12,A13,A14,A15,AD7,AD6,AD5,AD4,AD3,AD2,AD1,AD0")
elseif(MINITEL_MODEL STREQUAL "nfz400")
  set(PINOUT "A15,A10,A11,A9,A8,A13,A14,A12,AD0,AD1,AD2,AD3,AD4,AD5,AD6,AD7")
elseif(MINITEL_MODEL MATCHES "justrom:(.*)")
  set(MINITEL_MODEL justrom)
  set(PINOUT "${CMAKE_MATCH_1}")
else()
  message(FATAL_ERROR "Invalid -DMINITEL_MODEL=... value")
endif()
message("Building for Minitel model ${MINITEL_MODEL}")
message("Using pinout ${PINOUT}")

# Select what features to support.
set(OPERATING_MODE "" CACHE STRING "Extra features to be enabled")
if(OPERATING_MODE STREQUAL "embedded")
  # Select the ROM to be embedded.
  set(EMBED_ROM_FILE "" CACHE FILEPATH "Path to the ROM to be emulated")
  if(NOT EMBED_ROM_FILE)
    message(FATAL_ERROR "Please set cmake ... -DEMBED_ROM_FILE=/path/to/rom.bin")
  endif()
  message("Embedding ROM binary at ${EMBED_ROM_FILE}")
else()
  message(FATAL_ERROR "Invalid -DOPERATING_MODE=... value")
endif()

add_subdirectory(src)
