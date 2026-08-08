#include "display.h"

#include <video/registers.h>

void display_setup(void) {
  VIDEO->ER0 = VIDEO_CMD_NOP;

  video_wait_busy();
  VIDEO->R1 = VIDEO_TGS_MODE_40L |
#ifdef VIDEO_TS9347
              VIDEO_TGS_SERVICE_SYNC_VSYNC_GEN_NP |
#endif
              VIDEO_TGS_BOARD_EXTRAS;
  VIDEO->ER0 = VIDEO_CMD_IND | VIDEO_IND_TGS;

  video_wait_busy();
  VIDEO->R1 = VIDEO_PAT_MODE_40L | VIDEO_PAT_FLASH_EN |
              VIDEO_PAT_INSERT_ACTIVE_AREA_MARK | VIDEO_PAT_CONCEAL_EN |
              VIDEO_PAT_BULK_EN | VIDEO_PAT_SERVICE_ROW_EN |
              VIDEO_PAT_BOARD_EXTRAS;
  VIDEO->ER0 = VIDEO_CMD_IND | VIDEO_IND_PAT;

  // Configure MAT and set the margin color to black.
  display_set_margin_color(0);

  video_wait_busy();
  VIDEO->R1 = 0x08;
  VIDEO->ER0 = VIDEO_CMD_IND | VIDEO_IND_ROR;

  video_wait_busy();
  VIDEO->R1 = 0x74;
  VIDEO->ER0 = VIDEO_CMD_IND | VIDEO_IND_DOR;

  // Enable VSYNC bit.
  VIDEO->ER0 = VIDEO_CMD_VRM;
  video_wait_busy();
}

void display_wait_vsync(void) {
  // Wait for VSYNC to *not* be in progress. This is necessary so that calling
  // this function twice back-to-back will actually wait the next frame instead
  // of returning immediately.
  while ((VIDEO->R0 & 0x04) == 0x00) {
  }

  // Wait for VSYNC to start (rising edge).
  while ((VIDEO->R0 & 0x04) == 0x04) {
  }

  // Note: The VSYNC bit will now stay low for two rows. We can return
  // immediately and let the CPU send video commands during these two rows.
}

void display_set_margin_color(uint8_t c) {
  video_wait_busy();
  VIDEO->R1 = VIDEO_MAT_DOUBLE_HEIGHT | VIDEO_MAT_CURSOR_FLASH_COMPLEMENTED |
              VIDEO_MAT_MARGIN_INSERT | VIDEO_MAT_MARGIN_COLOR(c);
  VIDEO->ER0 = VIDEO_CMD_IND | VIDEO_IND_MAT;
}

void display_write_char(uint8_t x, uint8_t y, uint8_t a, uint8_t b, uint8_t c) {
  video_wait_busy();
  VIDEO->R1 = c;
  VIDEO->R2 = b;
  VIDEO->R3 = a;
  VIDEO->R6 = y == 0 ? 0 : (7 + y);
  VIDEO->R7 = x;
  VIDEO->ER0 = VIDEO_CMD_TLM;
}

void display_write_text(uint8_t x, uint8_t y, uint8_t a, uint8_t b,
                        const char* text) {
  video_wait_busy();
  VIDEO->R1 = *text++;
  VIDEO->R2 = b;
  VIDEO->R3 = a;
  VIDEO->R6 = y == 0 ? 0 : (7 + y);
  VIDEO->R7 = x;
  VIDEO->ER0 = VIDEO_CMD_TLM | VIDEO_MEM_POSTINCR;
  while (*text != '\0') {
    video_wait_busy();
    VIDEO->ER1 = *text++;
  }
}

void display_clear_screen(uint8_t a) {
  for (uint8_t y = 0; y < DISPLAY_ROWS; y++) {
    display_clear_line(y, a);
  }
}

void display_clear_line(uint8_t y, uint8_t a) {
  video_wait_busy();
  VIDEO->R1 = ' ';
  VIDEO->R2 = 0x00;
  VIDEO->R3 = a;
  VIDEO->R6 = y == 0 ? 0 : (7 + y);
  VIDEO->R7 = 0;
  VIDEO->ER0 = VIDEO_CMD_TLM | VIDEO_MEM_POSTINCR;
  for (uint8_t x = 1; x < DISPLAY_COLS; x++) {
    video_wait_busy();
    VIDEO->ER0 = VIDEO_CMD_TLM | VIDEO_MEM_POSTINCR;
  }
}

void display_copy_line(uint8_t src_y, uint8_t dst_y) {
  video_wait_busy();
  VIDEO->R4 = src_y == 0 ? 0 : (7 + src_y);
  VIDEO->R5 = 0;
  VIDEO->R6 = dst_y == 0 ? 0 : (7 + dst_y);
  VIDEO->R7 = 0;
  VIDEO->ER0 = VIDEO_CMD_MVT | VIDEO_MOVE_DIR_AP_TO_MP | VIDEO_MOVE_STOP_EOB;
}
