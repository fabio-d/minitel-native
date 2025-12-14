#ifndef LIB_KEYBOARD_INCLUDE_KEYBOARD_KEYBOARD_H
#define LIB_KEYBOARD_INCLUDE_KEYBOARD_KEYBOARD_H

#include <stdint.h>

#include "board/definitions.h"
#include "keyboard/_generated_keymap.h"

// The following KEYBOARD_FOR_EACH_PRESSED_KEY definition is only used if the
// keyboard is organized as a matrix. Otherwise, it's the board-specific code
// that defines it using its board-specific knowledge.
#ifdef KEYBOARD_ROWS

// Iterates over the entire key code space in row order, reading the state of
// the corresponding keys from keyboard at the beginning of each row. The
// provided body of the loop will only be executed for keys that are pressed.
//
// Example usage:
//  bool up = false, down = false;
//  KEYBOARD_FOR_EACH_PRESSED_KEY(key) {
//    switch (key) {
//    case KEY_UP:
//      up = true;
//      break;
//    case KEY_DOWN:
//      down = true;
//      break;
//    }
//  }
//
// Arguments:
//  `key`: Name of a variable that will be defined by this macro and will
//  contain the key code of the pressed key (a KEY_* value) when the provided
//  body is executed.
#define KEYBOARD_FOR_EACH_PRESSED_KEY(key)                                    \
  for (uint8_t ___keyb_val = 0, key = 0x00; key < KEYBOARD_ROWS << 3; ++key)  \
    if (___keyb_val =                                                         \
            (key & 0x7) ? (___keyb_val >> 1) : board_read_keyboard(key >> 3), \
        (___keyb_val & 1) == 0)

// Low-level helper to assemble a key code from a (row, column) pair. Programs
// do not normally have to to this. Use the automatically-generated KEY_* macros
// instead!
#define KEYBOARD_MAKE_KEY_CODE(row, column) (((row) << 3) | column)

#endif

// Given a key code, return its name (a string starting with "KEY_"), or NULL if
// the code is not valid.
const char* board_key_to_name(uint8_t key);

#endif
