#include <8052.h>
#include <board/controls.h>
#include <stdbool.h>
#include <stdio.h>
#include <timer/timer.h>
#include <video/commands.h>
#include <video/mcu_interface.h>
#include <video/registers.h>

static uint8_t fifo[32];
static uint8_t fifo_pos = 0;
static uint8_t fifo_count = 0;

void serial_interrupt(void) __interrupt(SI0_VECTOR) {
  if (RI) {
    if (fifo_count != sizeof(fifo)) {
      fifo[(fifo_pos + fifo_count++) & (sizeof(fifo) - 1)] = SBUF;
    }
    RI = 0;
  }
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

  VIDEO->R1 = 0;
  VIDEO->ER0 = VIDEO_CMD_IND | VIDEO_IND_DOR;
  video_wait_busy();
}

static void display_set_mosaic() {
  for (uint8_t y = 0; y < 25; y++) {
    VIDEO->R6 = y == 0 ? 0 : (7 + y);
    VIDEO->R7 = 0;
    VIDEO->R0 = VIDEO_CMD_TLM | VIDEO_MEM_POSTINCR;
    for (uint8_t x = 0; x < 40; x++) {
      VIDEO->R3 = 0x70;
      VIDEO->R2 = 0x20;  // mosaic
      VIDEO->ER1 = 0;
      video_wait_busy();
    }
  }
}

void serial_setup(void) {
  const uint16_t rcap2 =
      TIMER_TICKS_TO_RELOAD_VALUE_16(TIMER_TICKS_FROM_BAUD_T2(BAUDRATE));

  T2CON = 0x30;
  RCAP2H = rcap2 >> 8;
  RCAP2L = rcap2 & 0xFF;
  SCON = 0x50;
  TR2 = 1;

  // Enable interrupts.
  ES = 1;
  EA = 1;
}

static uint8_t cur_x = 0;
static uint8_t cur_y = 0;

static void process_data(uint8_t data) {
  if (data == 0xff) {
    cur_x = cur_y = 0;
    VIDEO->R6 = 0;  // y
    VIDEO->R7 = 0;  // x
  } else {
    VIDEO->ER1 = data;
    video_wait_busy();

    if (cur_x++ == 39) {
      cur_x = 0;
      VIDEO->R7 = 0;

      if (cur_y++ == 0) {
        cur_y = 8;
      }
      VIDEO->R6 = cur_y;
    }
  }
}

void main(void) {
  display_setup();
  board_controls_set_defaults();

  // Prefill the display with attributes that select the mosaic character set.
  display_set_mosaic();

  // Start the character reception machinery.
  VIDEO->R0 = VIDEO_CMD_TBM | VIDEO_MEM_POSTINCR;
  process_data(0xFF);

  serial_setup();
  while (1) {
    bool have_data = false;
    uint8_t data;

    __critical {
      if (fifo_count != 0) {
        data = fifo[fifo_pos];
        have_data = true;

        if (++fifo_pos == sizeof(fifo)) {
          fifo_pos = 0;
        }
        fifo_count--;
      }
    }

    if (have_data) {
      process_data(data);
    }
  }
}
