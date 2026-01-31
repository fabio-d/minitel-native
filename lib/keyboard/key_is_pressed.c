#include <stddef.h>

#include "keyboard/keyboard.h"

#ifndef KEYBOARD_ROWS
bool keyboard_key_is_pressed(uint8_t key) {
  KEYBOARD_FOR_EACH_PRESSED_KEY(pressed_key) {
    if (pressed_key == key) {
      return true;
    }
  }
  return false;
}
#endif
