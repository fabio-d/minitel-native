add_library(minitel_timer INTERFACE)

target_include_directories(minitel_timer INTERFACE
  include/
)

target_link_libraries(minitel_timer INTERFACE
  minitel_board
)
