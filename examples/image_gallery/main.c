#include <8052.h>
#include <board/controls.h>
#include <keyboard/keyboard.h>
#include <stdbool.h>
#include <stdio.h>
#include <video/commands.h>
#include <video/mcu_interface.h>
#include <video/registers.h>

// clang-format off
#define MOSAIC_FLAG 0x200

#define FONT_BEGIN(identifier) \
  static __code uint8_t font_ ## identifier[] = {
#define FONT_GLYPH(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9) \
  s0, s1, s2, s3, s4, s5, s6, s7, s8, s9
#define FONT_END \
  };

#define SCREEN_BEGIN(identifier) \
  static __code uint16_t screen_ ## identifier[] = {
#define SCREEN_CUSTOM(code, bg, fg) \
  ((uint16_t)(bg) << 13) | ((uint16_t)(fg) << 10) | (code)
#define SCREEN_MOSAIC(code, bg, fg) \
  ((uint16_t)(bg) << 13) | ((uint16_t)(fg) << 10) | MOSAIC_FLAG | (code)
#define SCREEN_END \
  };
// clang-format on

#include "image1.h"
#include "image2.h"
#include "image3.h"
#include "image4.h"
#include "image5.h"
#include "image6.h"
#include "image7.h"
#include "image8.h"
#include "image9.h"

#define PRESSED_KEY_1 (1 << 0)
#define PRESSED_KEY_2 (1 << 1)
#define PRESSED_KEY_3 (1 << 2)
#define PRESSED_KEY_4 (1 << 3)
#define PRESSED_KEY_5 (1 << 4)
#define PRESSED_KEY_6 (1 << 5)
#define PRESSED_KEY_7 (1 << 6)
#define PRESSED_KEY_8 (1 << 7)
#define PRESSED_KEY_9 (1 << 8)
#define PRESSED_KEY_PREVIOUS (1 << 9)
#define PRESSED_KEY_NEXT (1 << 10)

static uint16_t get_pressed_keys(void) {
  uint16_t result = 0;

  KEYBOARD_FOR_EACH_PRESSED_KEY(key) {
    switch (key) {
      case KEY_1:
        result |= PRESSED_KEY_1;
        break;
      case KEY_2:
        result |= PRESSED_KEY_2;
        break;
      case KEY_3:
        result |= PRESSED_KEY_3;
        break;
      case KEY_4:
        result |= PRESSED_KEY_4;
        break;
      case KEY_5:
        result |= PRESSED_KEY_5;
        break;
      case KEY_6:
        result |= PRESSED_KEY_6;
        break;
      case KEY_7:
        result |= PRESSED_KEY_7;
        break;
      case KEY_8:
        result |= PRESSED_KEY_8;
        break;
      case KEY_9:
        result |= PRESSED_KEY_9;
        break;
      case KEY_PREVIOUS:
        result |= PRESSED_KEY_PREVIOUS;
        break;
      case KEY_NEXT:
        result |= PRESSED_KEY_NEXT;
        break;
    }
  }

  return result;
}

static void display_setup(void) {
  VIDEO->ER0 = VIDEO_CMD_NOP;
  video_wait_busy();

  VIDEO->R1 = VIDEO_TGS_MODE_40L | VIDEO_TGS_BOARD_EXTRAS;
  VIDEO->ER0 = VIDEO_CMD_IND | VIDEO_IND_TGS;
  video_wait_busy();

  VIDEO->R1 = VIDEO_PAT_MODE_40L | VIDEO_PAT_FLASH_EN |
              VIDEO_PAT_INSERT_ACTIVE_AREA_MARK | VIDEO_PAT_CONCEAL_EN |
              VIDEO_PAT_BULK_EN | VIDEO_PAT_SERVICE_ROW_EN |
              VIDEO_PAT_BOARD_EXTRAS;
  VIDEO->ER0 = VIDEO_CMD_IND | VIDEO_IND_PAT;
  video_wait_busy();

  VIDEO->R1 = VIDEO_MAT_CURSOR_FLASH_COMPLEMENTED | VIDEO_MAT_MARGIN_INSERT |
              VIDEO_MAT_MARGIN_COLOR(0);
  VIDEO->ER0 = VIDEO_CMD_IND | VIDEO_IND_MAT;
  video_wait_busy();

  VIDEO->R1 = 0x08;
  VIDEO->ER0 = VIDEO_CMD_IND | VIDEO_IND_ROR;
  video_wait_busy();

  VIDEO->R1 = 0b00100110;  // G'1 at D=1 B=0, G'0 at D=1 B=2
  VIDEO->ER0 = VIDEO_CMD_IND | VIDEO_IND_DOR;
  video_wait_busy();
}

static void load_font(__code uint8_t *font, size_t font_len) {
  uint8_t z = 0b0100;  // D=1 B=0
  uint8_t c = 0;
  uint8_t sn = 0;

  for (size_t i = 0; i < font_len; i++) {
    uint8_t z0 = (z & (1 << 0)) != 0;
    uint8_t z1 = (z & (1 << 1)) != 0;
    uint8_t z2 = (z & (1 << 2)) != 0;
    uint8_t z3 = (z & (1 << 3)) != 0;

    VIDEO->R1 = font[i];
    VIDEO->R4 = (z2 << 5) | (c >> 2);
    VIDEO->R5 = (z0 << 7) | (z1 << 6) | (sn << 2) | (c & 0b11);
    VIDEO->R6 = (z3 << 6);
    VIDEO->ER0 = VIDEO_CMD_TBA;
    video_wait_busy();

    if (++sn == 10) {
      sn = 0;

      switch (++c) {
        case 4:
          c = 32;
          break;
        case 128:
          c = 0;
          z++;
          break;
      }
    }
  }
}

static void expand_code_index(uint16_t idx, uint8_t *b, uint8_t *c) {
  if (idx & MOSAIC_FLAG) {
    *b = 0x20;  // G10 (mosaic)
    *c = idx & 0x7F;
  } else if (idx < 4) {
    *b = 0xA0;  // G'10
    *c = idx;
  } else if (idx < 100) {
    *b = 0xA0;  // G'10
    *c = 32 + idx - 4;
  } else if (idx < 104) {
    *b = 0xB0;  // G'11
    *c = idx - 100;
  } else if (idx < 200) {
    *b = 0xB0;  // G'11
    *c = 32 + idx - 104;
  } else if (idx < 204) {
    *b = 0x80;  // G'0
    *c = idx - 200;
  } else if (idx < 300) {
    *b = 0x80;  // G'0
    *c = 32 + idx - 204;
  } else {
    // this should never happen
    *b = 0;
    *c = '!';
  }
}

static void load_screen(__code uint16_t *screen) {
  uint16_t idx = 0;

  for (uint8_t y = 0; y < 25; y++) {
    VIDEO->R6 = y == 0 ? 0 : (7 + y);
    VIDEO->R7 = 0;
    VIDEO->R0 = VIDEO_CMD_TLM | VIDEO_MEM_POSTINCR;
    for (uint8_t x = 0; x < 40; x++) {
      uint8_t bg = (screen[idx] >> 13) & 0x7;
      uint8_t fg = (screen[idx] >> 10) & 0x7;

      uint8_t b, c;
      expand_code_index(screen[idx] & 0x3FF, &b, &c);

      VIDEO->R3 = bg | (fg << 4);
      VIDEO->R2 = b;
      VIDEO->ER1 = c;
      video_wait_busy();

      idx++;
    }
  }
}

static void display_image(uint8_t image_num) {
  switch (image_num) {
    case 1:
      load_screen(screen_image1);
      load_font(font_image1, sizeof(font_image1));
      break;
    case 2:
      load_screen(screen_image2);
      load_font(font_image2, sizeof(font_image2));
      break;
    case 3:
      load_screen(screen_image3);
      load_font(font_image3, sizeof(font_image3));
      break;
    case 4:
      load_screen(screen_image4);
      load_font(font_image4, sizeof(font_image4));
      break;
    case 5:
      load_screen(screen_image5);
      load_font(font_image5, sizeof(font_image5));
      break;
    case 6:
      load_screen(screen_image6);
      load_font(font_image6, sizeof(font_image6));
      break;
    case 7:
      load_screen(screen_image7);
      load_font(font_image7, sizeof(font_image7));
      break;
    case 8:
      load_screen(screen_image8);
      load_font(font_image8, sizeof(font_image8));
      break;
    case 9:
      load_screen(screen_image9);
      load_font(font_image9, sizeof(font_image9));
      break;
  }
}

void main(void) {
  display_setup();
  board_controls_set_defaults();

  uint8_t image_num = 1;
  display_image(image_num);

  uint16_t prev_keys = 0;
  while (true) {
    uint8_t next_image_num = image_num;

    // Handle keypresses.
    uint16_t curr_keys = get_pressed_keys();
    if (curr_keys && !prev_keys) {
      if (curr_keys & PRESSED_KEY_1) {
        next_image_num = 1;
      } else if (curr_keys & PRESSED_KEY_2) {
        next_image_num = 2;
      } else if (curr_keys & PRESSED_KEY_3) {
        next_image_num = 3;
      } else if (curr_keys & PRESSED_KEY_4) {
        next_image_num = 4;
      } else if (curr_keys & PRESSED_KEY_5) {
        next_image_num = 5;
      } else if (curr_keys & PRESSED_KEY_6) {
        next_image_num = 6;
      } else if (curr_keys & PRESSED_KEY_7) {
        next_image_num = 7;
      } else if (curr_keys & PRESSED_KEY_8) {
        next_image_num = 8;
      } else if (curr_keys & PRESSED_KEY_9) {
        next_image_num = 9;
      } else if (curr_keys & PRESSED_KEY_NEXT) {
        if (++next_image_num == 10) {
          next_image_num = 1;
        }
      } else if (curr_keys & PRESSED_KEY_PREVIOUS) {
        if (--next_image_num == 0) {
          next_image_num = 9;
        }
      }
    }
    prev_keys = curr_keys;

    if (next_image_num != image_num) {
      image_num = next_image_num;
      display_image(image_num);
    }
  }
}
