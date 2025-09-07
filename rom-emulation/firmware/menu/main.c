#include <8052.h>
#include <board/controls.h>
#include <board/definitions.h>
#include <keyboard/keyboard.h>
#include <stdbool.h>
#include <stdio.h>
#include <timer/timer.h>
#include <video/commands.h>
#include <video/mcu_interface.h>
#include <video/registers.h>

#include "magic-io.h"

#define ATTR_WHITE_ON_BLACK 0x07
#define ATTR_BLACK_ON_WHITE 0x47

// Initializes the serial port ("peri-informatique") at 2400 baud 8N1.
void serial_setup(void) {
  const uint16_t rcap2 =
      TIMER_TICKS_TO_RELOAD_VALUE_16(TIMER_TICKS_FROM_BAUD_T2(2400));
  T2CON = 0x30;
  RCAP2H = rcap2 >> 8;
  RCAP2L = rcap2 & 0xFF;
  SCON = 0x50;
  TR2 = 1;
}

// Initializes the video chip in 40 columns short mode.
static void video_setup(void) {
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

  VIDEO->R1 = 0x00;
  VIDEO->ER0 = VIDEO_CMD_IND | VIDEO_IND_DOR;
  video_wait_busy();

  VIDEO->R1 = 0x08;
  VIDEO->ER0 = VIDEO_CMD_IND | VIDEO_IND_ROR;
  video_wait_busy();
}

// Set the attribute byte for the next emitted character and for clearing.
static void video_set_attributes(uint8_t attributes) { VIDEO->R1 = attributes; }

// Clears the given rectangle.
static void video_clear(uint8_t from_x, uint8_t to_x, uint8_t from_y,
                        uint8_t to_y) {
  VIDEO->R2 = ' ';
  for (uint8_t y = from_y; y <= to_y; y++) {
    VIDEO->R6 = y == 0 ? 0 : (7 + y);
    VIDEO->R7 = 0;
    for (uint8_t x = from_x; x <= to_x; x++) {
      VIDEO->ER0 = VIDEO_CMD_TSM | VIDEO_MEM_POSTINCR;
      video_wait_busy();
    }
  }
}

// Sets the cursor position, i.e. where the next emitted character will be
// printed.
static void video_set_cursor(uint8_t x, uint8_t y) {
  VIDEO->R7 = x;
  VIDEO->R6 = y == 0 ? 0 : (7 + y);
}

// Connect stdout to video output.
int putchar(int c) {
  VIDEO->R0 = VIDEO_CMD_TSM | VIDEO_MEM_POSTINCR;
  VIDEO->ER2 = c;
  video_wait_busy();
  return c;
}

static void run_main_menu(void) {
  video_set_attributes(ATTR_WHITE_ON_BLACK);
  video_set_cursor(0, 2);
  printf("Press ");
  video_set_attributes(ATTR_BLACK_ON_WHITE);
  printf(" B ");
  video_set_attributes(ATTR_WHITE_ON_BLACK);
  printf(" to boot the stored ROM");

  video_set_cursor(0, 22);
  printf("Press ");
  video_set_attributes(ATTR_BLACK_ON_WHITE);
  printf(" S ");
  video_set_attributes(ATTR_WHITE_ON_BLACK);
  printf(" to enter serial client mode");

  while (magic_io_get_desired_state() == MAGIC_IO_DESIRED_STATE_MAIN_MENU) {
    KEYBOARD_FOR_EACH_PRESSED_KEY(key) {
      switch (key) {
        case KEY_B:
          magic_io_signal_user_requested_boot();
          break;
        case KEY_S:
          magic_io_signal_user_client_mode();
          break;
      }
    }
  }

  video_clear(0, 39, 2, 22);
}

static void run_boot_trampoline(void) {
  video_set_attributes(ATTR_WHITE_ON_BLACK);
  video_set_cursor(0, 2);
  printf("Booting...");

  magic_io_jump_to_trampoline();
}

static void run_partition_error(void) {
  video_set_attributes(ATTR_WHITE_ON_BLACK);
  video_set_cursor(0, 2);
  printf("Error! The Pico is not partitioned properly.");

  while (magic_io_get_desired_state() ==
         MAGIC_IO_DESIRED_STATE_PARTITION_ERROR) {
  }

  video_clear(0, 39, 2, 2);
}

static void run_client_mode(void) {
  video_set_attributes(ATTR_WHITE_ON_BLACK);
  video_set_cursor(0, 2);
  printf("Serial client mode is active.");
  video_set_cursor(0, 4);
  printf("Running at 2400 baud, 8N1.");

  TI = 1;

  while (magic_io_get_desired_state() == MAGIC_IO_DESIRED_STATE_CLIENT_MODE) {
    uint8_t value;

    // Serial-to-emulator.
    if (RI) {
      value = SBUF;
      RI = 0;

      magic_io_tx_byte(value);
    }

    // Emulator-to-serial.
    if (TI && magic_io_rx_byte(&value)) {
      TI = 0;
      SBUF = value;
    }
  }

  while (!TI) {
  }
  TI = 0;

  video_clear(0, 39, 2, 4);
}

void main(void) {
  serial_setup();
  video_setup();
  board_controls_set_defaults();
  magic_io_reset();

  video_set_attributes(ATTR_WHITE_ON_BLACK);
  video_clear(0, 39, 0, 24);
  video_set_cursor(0, 0);
  printf("Minitel ROM Emulator");

  while (true) {
    switch (magic_io_get_desired_state()) {
      case MAGIC_IO_DESIRED_STATE_MAIN_MENU:
        run_main_menu();
        break;
      case MAGIC_IO_DESIRED_STATE_BOOT_TRAMPOLINE:
        run_boot_trampoline();
        break;
      case MAGIC_IO_DESIRED_STATE_PARTITION_ERROR:
        run_partition_error();
        break;
      case MAGIC_IO_DESIRED_STATE_CLIENT_MODE:
        run_client_mode();
        break;
    }
  }
}
