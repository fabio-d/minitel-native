add_library(minitel_video INTERFACE)

target_include_directories(minitel_video INTERFACE
  include/
)

target_link_libraries(minitel_video INTERFACE
  minitel_board
)
