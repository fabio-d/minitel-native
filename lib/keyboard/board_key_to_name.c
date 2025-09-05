#include <stddef.h>

#include "keyboard/keyboard.h"

const char *board_key_to_name(uint8_t key) {
  switch (key) {
#define KEYLIST_ENTRY(name) \
  case KEY_##name:          \
    return "KEY_" #name;
#include "keyboard/_generated_keylist.h"
#undef KEYLIST_ENTRY
    default:
      return NULL;
  }
}
