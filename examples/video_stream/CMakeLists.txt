cmake_minimum_required(VERSION 3.13)
include(../../lib/cmake/init.cmake)

project(video_stream C)
minitel_lib_init()

# These are the high baud rates that were found to be working for each model.
if(MINITEL_MODEL STREQUAL "nfz330")
  set(BAUDRATE 57600)
elseif(MINITEL_MODEL STREQUAL "nfz400")
  set(BAUDRATE 111860)  # fine-tuned to match the USB-UART's 115200 rate
else()
  message(FATAL_ERROR "Unsupported MINITEL_MODEL")
endif()

add_executable(video_stream
  main.c
)

target_compile_definitions(video_stream PRIVATE
  "-DBAUDRATE=${BAUDRATE}"
)

target_link_libraries(video_stream PRIVATE
  minitel_board
  minitel_timer
  minitel_video
)

minitel_add_bin_output(video_stream)
