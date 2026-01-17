add_library(minitel_timer
  timer.c
)

target_include_directories(minitel_timer PUBLIC
  include/
)

target_link_libraries(minitel_timer PUBLIC
  minitel_board
)
