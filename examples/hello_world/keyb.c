#include "keyb.h"

#include <keyboard/keyboard.h>
#include <stdio.h>
#include <string.h>

#include "display.h"

#ifdef KEYBOARD_ROWS  // generic keyboard, organized as a matrix

void keyb_widget_prepare(uint8_t x0, uint8_t y0) {
  draw_rectangle(x0, y0, x0 + 18, y0 + 2 + KEYBOARD_ROWS + 1);
  draw_string(x0 + 2, y0, "Keyboard Matrix", 0x07);
  draw_string(x0 + 3, y0 + 1, "7 6 5 4 3 2 1 0", 0x07);
  for (uint8_t r = 0; r < KEYBOARD_ROWS; r++) {
    char buf[4];
    sprintf(buf, "%d\x0e", r);
    draw_string(x0 + 1, y0 + 2 + r, buf, 0x07);
  }
}

// Note: this function reads the keyboard state using the low-level
// board_read_keyboard() function, that does not abstract the hardware's
// keyboard matrix layout. Programs that just need to know what keys are
// currently pressed can use the higher-level KEYBOARD_FOR_EACH_PRESSED_KEY
// macro instead.
void keyb_widget_fill(uint8_t x0, uint8_t y0) {
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

#elif defined(BOARD_722039M)  // special case for the 722039M

static uint8_t cur_x, cur_y;

void keyb_widget_prepare(uint8_t x0, uint8_t y0) {
  draw_rectangle(x0, y0, x0 + 18, y0 + 12);
  draw_string(x0 + 2, y0, "Keyboard Stream", 0x07);
  cur_x = cur_y = 0;
}

void keyb_widget_fill(uint8_t x0, uint8_t y0) {
  uint8_t val;
  if (board_read_keyboard_raw_stream(&val)) {
    if (cur_x == 6) {
      // Scroll, if necessary and go to the next line.
      if (cur_y == 9) {
        for (uint8_t i = 0; i < cur_y; i++) {
          draw_copy_until_end_of_line(x0 + 1, y0 + 2 + i, y0 + 1 + i);
        }
        draw_string(x0 + 1, y0 + 1 + cur_y, "                 ", 0x07);
      } else {
        cur_y++;
      }
      cur_x = 0;
    }

    char buf[3];
    sprintf(buf, "%02x", val);
    draw_string(x0 + 1 + cur_x++ * 3, y0 + 1 + cur_y, buf, 0x07);
  }

  const char *pressed_modifier = " ";
  switch (board_read_keyboard_modifier()) {
    case KEY_FUNCTION:
      pressed_modifier = "F";
      break;
    case KEY_CONTROL:
      pressed_modifier = "C";
      break;
    case KEY_SHIFT:
      pressed_modifier = "S";
      break;
  }
  draw_string(x0 + 17, y0 + 11, pressed_modifier, 0x03);

  uint8_t keycode = board_read_keyboard_key();
  const char *pressed_key = board_key_to_name(keycode);
  if (pressed_key != NULL) {
    draw_string(x0 + 1, y0 + 11, pressed_key, 0x07);
    for (uint8_t x = x0 + 1 + strlen(pressed_key); x < x0 + 17; x++)
      draw_string(x, y0 + 11, " ", 0);
  }
}

#endif
