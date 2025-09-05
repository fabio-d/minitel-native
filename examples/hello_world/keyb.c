#include "keyb.h"

#include <keyboard/keyboard.h>
#include <stdio.h>
#include <string.h>

#include "display.h"

void keyb_matrix_prepare(uint8_t x0, uint8_t y0) {
  draw_rectangle(x0, y0, x0 + 18, y0 + 2 + KEYBOARD_ROWS + 1);
  draw_string(x0 + 2, y0, "Keyboard Matrix", 0x07);
  draw_string(x0 + 3, y0 + 1, "7 6 5 4 3 2 1 0", 0x07);
  for (uint8_t r = 0; r < KEYBOARD_ROWS; r++) {
    char buf[4];
    sprintf(buf, "%d\x0e", r);
    draw_string(x0 + 1, y0 + 2 + r, buf, 0x07);
  }
}

void keyb_matrix_fill(uint8_t x0, uint8_t y0) {
  const char *pressed_key = NULL;

  for (uint8_t r = 0; r < KEYBOARD_ROWS; r++) {
    uint8_t values = board_read_keyboard(r);
    uint8_t x = x0 + 3;
    for (uint8_t c = 8; c-- != 0;) {
      bool is_one = (values & (1 << c)) != 0;
      draw_string(x, y0 + 2 + r, is_one ? "1" : "0", is_one ? 0x02 : 0x07);
      x += 2;

      if (!is_one) {
        if (pressed_key != NULL) {
          pressed_key = "(multiple keys)";
        } else {
          uint8_t key = KEYBOARD_MAKE_KEY_CODE(r, c);
          pressed_key = board_key_to_name(key);
          if (pressed_key == NULL) {
            pressed_key = "(unknown key)";
          }
        }
      }
    }
  }

  // Draw new text and clear the previous text, if any.
  if (pressed_key != NULL) {
    draw_string(x0 + 1, y0 + 2 + KEYBOARD_ROWS, pressed_key, 0x06);
  } else {
    pressed_key = "";
  }

  for (uint8_t x = x0 + 1 + strlen(pressed_key); x < x0 + 18; x++)
    draw_string(x, y0 + 2 + KEYBOARD_ROWS, " ", 0);
}
