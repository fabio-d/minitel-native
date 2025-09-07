cmake_minimum_required(VERSION 3.13)
include(../../../lib/cmake/init.cmake)

project(menu-rom C)
minitel_lib_init()

add_executable(menu-rom
  magic-io.c
  main.c
)

target_include_directories(menu-rom PRIVATE
  ../common
)

target_link_libraries(menu-rom PRIVATE
  minitel_board
  minitel_keyboard
  minitel_timer
  minitel_video
)

minitel_add_bin_output(menu-rom)
