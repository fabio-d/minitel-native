add_library(minitel_board
  controls.c
  read_keyboard.c
)

target_compile_definitions(minitel_board PUBLIC
  -DBOARD_NFZ330
)

target_include_directories(minitel_board PUBLIC
  include/
)
