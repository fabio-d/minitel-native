#include "display.h"

#include <video/commands.h>
#include <video/mcu_interface.h>
#include <video/registers.h>

// Colors in grayscale order.
static const uint8_t grayscale[8] = {0, 4, 1, 5, 2, 6, 3, 7};

void display_setup(void) {
  VIDEO->ER0 = VIDEO_CMD_NOP;

  video_wait_busy();
  VIDEO->R1 = VIDEO_TGS_MODE_40S | VIDEO_TGS_BOARD_EXTRAS;
  VIDEO->ER0 = VIDEO_CMD_IND | VIDEO_IND_TGS;

  video_wait_busy();
  VIDEO->R1 = VIDEO_PAT_MODE_40S | VIDEO_PAT_FLASH_EN |
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
  VIDEO->R1 = 0x00;
  VIDEO->ER0 = VIDEO_CMD_IND | VIDEO_IND_DOR;
}

void clear_line(uint8_t y) {
  video_wait_busy();
  VIDEO->R1 = 0x07;  // Attributes
  VIDEO->R2 = ' ';
  VIDEO->R6 = y == 0 ? 0 : (7 + y);
  VIDEO->R7 = 0;
  VIDEO->ER0 = VIDEO_CMD_TSM | VIDEO_MEM_POSTINCR;
  for (uint8_t c = 1; c < 40; ++c) {
    video_wait_busy();
    VIDEO->ER0 = VIDEO_CMD_TSM | VIDEO_MEM_POSTINCR;
  }
}

void clear_full(void) {
  for (uint8_t r = 0; r < 25; r++) {
    clear_line(r);
  }
}

void draw_string(uint8_t x, uint8_t y, const char *text, uint8_t attributes) {
  if (*text == '\0') {
    return;  // empty string: nothing to do
  }

  video_wait_busy();
  VIDEO->R1 = attributes;
  VIDEO->R7 = x;
  VIDEO->R6 = y == 0 ? 0 : (7 + y);
  VIDEO->R0 = VIDEO_CMD_TSM | VIDEO_MEM_POSTINCR;
  VIDEO->ER2 = *text++;
  while (*text != '\0') {
    video_wait_busy();
    VIDEO->ER2 = *text++;
  }
}

void draw_rectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
  // Print corners
  draw_string(x1, y1, "\x5c", 0x82);
  draw_string(x2, y1, "\x6c", 0x82);
  draw_string(x1, y2, "\x4d", 0x82);
  draw_string(x2, y2, "\x4e", 0x82);

  // Print horizontal lines
  for (uint8_t x = x1 + 1; x < x2; x++) {
    draw_string(x, y1, "\x4c", 0x81);
    draw_string(x, y2, "\x4c", 0x81);
  }

  // Print vertical lines
  for (uint8_t y = y1 + 1; y < y2; y++) {
    draw_string(x1, y, "\x55", 0x81);
    draw_string(x2, y, "\x6a", 0x81);
  }
}

void draw_color_pattern(uint8_t x0, uint8_t y0) {
  // Display a test pattern showing all the possible colors.
  draw_rectangle(x0, y0, x0 + 19, y0 + 12);
  draw_string(x0 + 4, y0, "Color Matrix", 0x07);

  // Column headers.
  for (uint8_t fg = 0; fg < 8; fg++) {
    uint8_t x = x0 + 3 + 2 * fg;

    // Show the top 4 bits corresponding to the foreground color.
    char buf[3] = {'x', '0' + grayscale[fg], '\0'};
    draw_string(x, y0 + 1, buf, 0x07);
  }

  // Main contents.
  for (uint8_t bg = 0; bg < 8; bg++) {
    uint8_t y = y0 + 2 + bg;

    // Show the top 4 bits corresponding to the background color.
    char buf[3] = {'0' + grayscale[bg], 'x', '\0'};
    draw_string(x0 + 1, y, buf, 0x07);

    for (uint8_t fg = 0; fg < 8; fg++) {
      uint8_t x = x0 + 3 + 2 * fg;

      // Two consecutive semigraphic characters showing a rectangle with a dash
      // inside.
      draw_string(x, y, "\x48\x44",
                  0x80 | (grayscale[bg] << 4) | grayscale[fg]);
    }
  }

  // Legend.
  draw_string(x0 + 3, y0 + 10, "\x48\x44", 0x86);
  draw_string(x0 + 6, y0 + 10, "= foreground", 0x06);
  draw_string(x0 + 3, y0 + 11, "\x77\x7b", 0x86);
  draw_string(x0 + 6, y0 + 11, "= background", 0x06);
}

void draw_copy_until_end_of_line(uint8_t x0, uint8_t y_src, uint8_t y_dst) {
  video_wait_busy();
  VIDEO->R7 = x0;
  VIDEO->R6 = y_src == 0 ? 0 : (7 + y_src);
  VIDEO->R5 = x0;
  VIDEO->R4 = y_dst == 0 ? 0 : (7 + y_dst);
  VIDEO->ER0 = VIDEO_CMD_MVD | VIDEO_MOVE_DIR_MP_TO_AP | VIDEO_MOVE_STOP_EOB;
}
