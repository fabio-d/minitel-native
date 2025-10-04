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

// Returns the pressed keys as flags.
//
// This shows the typical usage of the KEYBOARD_FOR_EACH_PRESSED_KEY macro: only
// the keys that actually matter to the program are handled, and the others are
// ignored by simply not listing them.
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

// Configures the video chip in 40-character long mode, and configure ROR and
// DOR so that:
// - The video RAM's first district (D=0) is interpreted as screen data:
//   - D=0 B=0 will contain the "C" byte of each character.
//   - D=0 B=1 will contain the "B" byte of each character.
//   - D=0 B=2 will contain the "A" byte of each character.
//   - D=0 B=3 will be unused.
// - The video RAM's second district (D=1) is interpreted as font data.
//   - D=1 B=0 will contain the G'10 font.
//   - D=1 B=1 will contain the G'11 font.
//   - D=1 B=2 will contain the G'0 font.
//   - D=1 B=3 will be unused.
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

  VIDEO->R1 = 0x08;  // D=0 B=0 (i.e. Z=0), YOR=8
  VIDEO->ER0 = VIDEO_CMD_IND | VIDEO_IND_ROR;
  video_wait_busy();

  VIDEO->R1 = 0x26;  // G'1 at D=1 B=0 (i.e. Z=4), G'0 at D=1 B=2 (i.e. Z=6)
  VIDEO->ER0 = VIDEO_CMD_IND | VIDEO_IND_DOR;
  video_wait_busy();
}

// Loads the given font data sequentially into the second district.
//
// Each 10x8 glyph takes 10 bytes and each block can contain 100 glyphs.
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

    // If we have just written the last scanline of the current glyph, move to
    // the next one.
    if (++sn == 10) {
      sn = 0;

      // Skip the gap between character 3 and character 32 (only characters 0-3
      // and 32-127 exist in the video chip - hence, 100 in total).
      switch (++c) {
        case 4:
          c = 32;
          break;
        case 128:
          c = 0;
          z++;  // We are at the end of the block, move to the next one.
          break;
      }
    }
  }
}

// This maps a glyph index in the custom font definition to the non-linear
// character code into which the glyph has been loaded.
//
// - Indices 0-99 map to G'10.
// - Indices 100-199 map to G'11.
// - Indices 200-299 map to G'0.
// - As a special case, indices marked with MOSAIC_FLAG are mapped to G10 (i.e.
//   the built-in mosaic character set).
//
// Reminder: custom fonts' character codes have a discontinuity between 4 and
// 31! Only character codes 0-3 and 32-127 exist in the video chip.
static void expand_code_index(uint16_t idx, uint8_t *b, uint8_t *c) {
  if (idx & MOSAIC_FLAG) {  // mosaic -> G10
    *b = 0x20;
    *c = idx & 0x7F;
  } else if (idx < 4) {  // 0-3 -> G'10, starting from 0
    *b = 0xA0;
    *c = idx;
  } else if (idx < 100) {  // 4-99 -> G'10, starting from 32
    *b = 0xA0;
    *c = 32 + idx - 4;
  } else if (idx < 104) {  // 100-103 -> G'11, starting from 0
    *b = 0xB0;
    *c = idx - 100;
  } else if (idx < 200) {  // 104-199 -> G'11, starting from 32
    *b = 0xB0;
    *c = 32 + idx - 104;
  } else if (idx < 204) {  // 200-203 -> G'0, starting from 0
    *b = 0x80;
    *c = idx - 200;
  } else if (idx < 300) {  // 204-299 -> G'0, starting from 32
    *b = 0x80;
    *c = 32 + idx - 204;
  } else {  // this should never happen
    *b = 0;
    *c = '!';
  }
}

// Fill the screen sequentially with the given data.
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

  // Initially display the first image.
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

    // Draw the new image when the selection changes.
    if (next_image_num != image_num) {
      image_num = next_image_num;
      display_image(image_num);
    }
  }
}
