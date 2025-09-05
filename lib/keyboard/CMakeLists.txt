add_library(minitel_keyboard
  board_key_to_name.c
)

target_include_directories(minitel_keyboard PUBLIC
  include/
  ${CMAKE_CURRENT_BINARY_DIR}/include/
)

target_link_libraries(minitel_keyboard PUBLIC
  minitel_board
)

set(KEYBOARD_MAP ${CMAKE_CURRENT_SOURCE_DIR}/../board/${MINITEL_MODEL}/keyboard_map.txt)

# Convert the keyboard map into C code.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/keyboard)
add_custom_command(
  OUTPUT
    include/keyboard/_generated_keylist.h
    include/keyboard/_generated_keymap.h
  COMMAND
    ${Python3_EXECUTABLE}
  ARGS
    ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate-keyboard-map.py
    ${KEYBOARD_MAP}
    include/keyboard/_generated_keylist.h
    include/keyboard/_generated_keymap.h
  DEPENDS ../scripts/generate-keyboard-map.py
  DEPENDS ${KEYBOARD_MAP}
  COMMENT "Generating keyboard map"
  VERBATIM
)
add_custom_target(minitel_keyboard-map
  DEPENDS
    include/keyboard/_generated_keylist.h
    include/keyboard/_generated_keymap.h
  SOURCES
    ../scripts/generate-keyboard-map.py
    ${KEYBOARD_MAP}
)
add_dependencies(minitel_keyboard minitel_keyboard-map)
