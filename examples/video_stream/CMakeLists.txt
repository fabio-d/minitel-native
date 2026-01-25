cmake_minimum_required(VERSION 3.13)
include(../../lib/cmake/init.cmake)

project(video_stream C)
minitel_lib_init()

add_executable(video_stream
  main.c
)

target_link_libraries(video_stream PRIVATE
  minitel_board
  minitel_keyboard
  minitel_timer
  minitel_video
)

minitel_add_bin_output(video_stream)
