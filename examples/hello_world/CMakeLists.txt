cmake_minimum_required(VERSION 3.13)
include(../../lib/cmake/init.cmake)

project(hello_world C)
minitel_lib_init()

add_executable(hello_world
  display.c
  keyb.c
  main.c
  ticks.c
)

target_link_libraries(hello_world PRIVATE
  minitel_board
  minitel_keyboard
  minitel_timer
  minitel_video
)

minitel_add_bin_output(hello_world)
