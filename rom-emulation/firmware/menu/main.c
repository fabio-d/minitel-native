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
#define ATTR_GRAY_ON_BLACK 0x02
#define ATTR_BLACK_ON_WHITE 0x47

// Initializes the timer 0 to overflow every 100 uS.
void timer_setup(void) {
  const uint8_t reload =
      TIMER_TICKS_TO_RELOAD_VALUE_8(TIMER_TICKS_FROM_US(100));
  TMOD = 0x02;
  TH0 = reload;
  TL0 = reload;
  TR0 = 1;
}

// Waits the given number of timer 0 overflow events, e.g. 10000 = 1 second.
void timer_delay(uint16_t ticks) {
  while (ticks-- != 0) {
    // Wait overflow.
    TF0 = 0;
    while (!TF0) {
    }
  }
}

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
  for (uint8_t i = 0; i < 16; i++) {
    __code const MAGIC_IO_CONFIGURATION_DATA_ROM_t* rom =
        magic_io_get_configuration_rom_slot(i);

    video_set_cursor(0, 2 + i);
    if (rom->is_present) {
      video_set_attributes(ATTR_BLACK_ON_WHITE);
      printf(" %X ", i);
      video_set_attributes(ATTR_WHITE_ON_BLACK);
      putchar(' ');
      for (uint8_t j = 0; j < rom->name_length && j < 36; j++) {
        putchar(rom->name[j]);
      }
    } else {
      video_set_attributes(ATTR_GRAY_ON_BLACK);
      printf("(slot %X is empty)", i);
    }
  }

  video_set_attributes(ATTR_WHITE_ON_BLACK);
  video_set_cursor(0, 22);
  printf("Press ");
  video_set_attributes(ATTR_BLACK_ON_WHITE);
  printf(" S ");
  video_set_attributes(ATTR_WHITE_ON_BLACK);
  printf(" to enter serial client mode");

  while (magic_io_get_desired_state() == MAGIC_IO_DESIRED_STATE_MAIN_MENU) {
    KEYBOARD_FOR_EACH_PRESSED_KEY(key) {
      switch (key) {
        case KEY_0:
          magic_io_signal_user_requested_boot(0);
          break;
        case KEY_1:
          magic_io_signal_user_requested_boot(1);
          break;
        case KEY_2:
          magic_io_signal_user_requested_boot(2);
          break;
        case KEY_3:
          magic_io_signal_user_requested_boot(3);
          break;
        case KEY_4:
          magic_io_signal_user_requested_boot(4);
          break;
        case KEY_5:
          magic_io_signal_user_requested_boot(5);
          break;
        case KEY_6:
          magic_io_signal_user_requested_boot(6);
          break;
        case KEY_7:
          magic_io_signal_user_requested_boot(7);
          break;
        case KEY_8:
          magic_io_signal_user_requested_boot(8);
          break;
        case KEY_9:
          magic_io_signal_user_requested_boot(9);
          break;
        case KEY_A:
          magic_io_signal_user_requested_boot(10);
          break;
        case KEY_B:
          magic_io_signal_user_requested_boot(11);
          break;
        case KEY_C:
          magic_io_signal_user_requested_boot(12);
          break;
        case KEY_D:
          magic_io_signal_user_requested_boot(13);
          break;
        case KEY_E:
          magic_io_signal_user_requested_boot(14);
          break;
        case KEY_F:
          magic_io_signal_user_requested_boot(15);
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

static void run_empty_slot_error(void) {
  video_set_attributes(ATTR_WHITE_ON_BLACK);
  video_set_cursor(0, 2);
  printf("Error! The requested ROM slot is empty.");

  while (magic_io_get_desired_state() ==
         MAGIC_IO_DESIRED_STATE_EMPTY_SLOT_ERROR) {
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
  timer_setup();
  serial_setup();
  video_setup();
  board_controls_set_defaults();

  // Wait 2 seconds before displaying any non-black pixel, to give the CRT some
  // time to settle.
  video_set_attributes(ATTR_WHITE_ON_BLACK);
  video_clear(0, 39, 0, 24);
  timer_delay(20000);

  video_set_cursor(0, 0);
  printf("Minitel ROM Emulator");

  magic_io_reset();

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
      case MAGIC_IO_DESIRED_STATE_EMPTY_SLOT_ERROR:
        run_empty_slot_error();
        break;
      case MAGIC_IO_DESIRED_STATE_CLIENT_MODE:
        run_client_mode();
        break;
    }
  }
}
