#include <8052.h>
#include <board/controls.h>
#include <keyboard/keyboard.h>
#include <stdbool.h>
#include <stdio.h>
#include <timer/timer.h>
#include <video/commands.h>
#include <video/mcu_interface.h>
#include <video/registers.h>

// Serial input FIFO.
static __idata uint8_t fifo[64];
static uint8_t fifo_rpos = 0;
static uint8_t fifo_wpos = 0;

// Previous status value, for detecting the VSYNC falling edge.
static uint8_t prev_status;

// Triple buffering.
static bool buf_enable = true;  // enable triple buffering
static int8_t bufidx_input;     // buffer being received
static int8_t bufidx_ready;     // buffer ready to be displayed

// Base MP values for districts 0 and 1.
static const uint8_t D0_R6 = 0x00;
static const uint8_t D1_R6 = 0x20;

// Base MP values for each block number.
static const uint8_t B0_R7 = 0x00;
static const uint8_t B1_R7 = 0x80;
static const uint8_t B2_R7 = 0x40;
static const uint8_t B3_R7 = 0xC0;

// ROR values for (D=0, B=0), (D=0, B=2), (D=1, B=0)
static const uint8_t RORs[3] = {0x08, 0x48, 0x28};

// Command that is maintained in R0 while idle.
static const uint8_t CMD_IDLE = VIDEO_CMD_TBM | VIDEO_MEM_POSTINCR;

// Serial interrupt handler, which simply puts each received byte into the FIFO.
void serial_interrupt(void) __interrupt(SI0_VECTOR) __using(1) {
  if (RI) {
    uint8_t val = SBUF;
    RI = 0;

    uint8_t fifo_wpos_next = fifo_wpos + 1;
    if (fifo_wpos_next == sizeof(fifo)) {
      fifo_wpos_next = 0;
    }

    if (fifo_wpos_next != fifo_rpos) {
      fifo[fifo_wpos] = val;
      fifo_wpos = fifo_wpos_next;
    }
  }
}

// Some boards require the board-specific "board_periodic_task()" function to be
// called at a fixed rate.
#ifdef BOARD_PERIODIC_TASK_HZ
static inline void board_periodic_task_reload() {
  const uint16_t reload_value = TIMER_TICKS_TO_RELOAD_VALUE_16(
      TIMER_TICKS_FROM_HZ(BOARD_PERIODIC_TASK_HZ));
  timer_adjust_thtl1(reload_value + TIMER_ADJUST_THTL_CYCLES);
}

void board_periodic_task_interrupt(void) __interrupt(TF1_VECTOR) {
  board_periodic_task_reload();
  board_periodic_task();
}

static void board_periodic_task_setup(void) {
  board_periodic_task_reload();

  // Set Timer1 in mode 1 and start it.
  TMOD = (TMOD & T0_MASK) | T1_M0;
  TR1 = 1;

  // Enable Timer1 interrupt.
  ET1 = 1;
}
#endif

#define PRESSED_KEY_CONNECT (1 << 0)
#define PRESSED_KEY_PREVIOUS (1 << 1)
#define PRESSED_KEY_NEXT (1 << 2)
#define PRESSED_KEY_SPACE (1 << 3)

// Returns the pressed keys as flags.
static uint8_t get_pressed_keys(void) {
  uint8_t result = 0;

  KEYBOARD_FOR_EACH_PRESSED_KEY(key) {
    switch (key) {
      case KEY_CONNECT:
        result |= PRESSED_KEY_CONNECT;
        break;
      case KEY_PREVIOUS:
        result |= PRESSED_KEY_PREVIOUS;
        break;
      case KEY_NEXT:
        result |= PRESSED_KEY_NEXT;
        break;
      case KEY_SPACE:
        result |= PRESSED_KEY_SPACE;
        break;
    }
  }

  return result;
}

void serial_start(uint16_t rcap2) {
  // Set up the serial port to be timed from Timer 2 and enable reception.
  T2CON = 0x30;
  RCAP2H = rcap2 >> 8;
  RCAP2L = rcap2 & 0xFF;
  SCON = 0x50;
  TR2 = 1;

  // Enable interrupts.
  ES = 1;
}

void serial_stop(void) {
  // Disable interrupts.
  ES = 0;

  // Stop Timer 2.
  TR2 = 0;
}

// Configures the video chip in 40-character short mode.
static void display_setup(void) {
  VIDEO->ER0 = VIDEO_CMD_NOP;
  video_wait_busy();

  VIDEO->R1 = VIDEO_TGS_MODE_40S |
#ifdef VIDEO_TS9347
              VIDEO_TGS_SERVICE_SYNC_VSYNC_GEN_NP |
#endif
              VIDEO_TGS_BOARD_EXTRAS;
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

  VIDEO->R1 = 0;
  VIDEO->ER0 = VIDEO_CMD_IND | VIDEO_IND_DOR;
  video_wait_busy();
}

// Fills the screen with blank mosaic characters. In particular:
// - B=base+0 (holding the "A*" bytes) will be filled with 0x87.
// - B=base+1 (holding the "B*" bytes) will be filled with 0x00.
static inline void display_set_mosaic(uint8_t r6_base, uint8_t r7_base) {
  VIDEO->R1 = 0x87;  // white foreground on black background, mosaic
  VIDEO->R2 = 0x00;  // blank (for now, overwritten during streaming)
  for (uint8_t y = 0; y < 25; y++) {
    VIDEO->R6 = r6_base | (y == 0 ? 0 : (7 + y));
    VIDEO->R7 = r7_base;
    for (uint8_t x = 0; x < 40; x++) {
      VIDEO->ER0 = VIDEO_CMD_TSM | VIDEO_MEM_POSTINCR;
      video_wait_busy();
    }
  }
}

static uint8_t cur_x;
static uint8_t cur_y;
static uint8_t base_r6;
static uint8_t base_r7;

// Each call to this function overwrites one byte of the screen memory's B*
// block. A* bytes will be left unaltered, with the values that were previously
// set by display_set_mosaic().
static inline void process_data(uint8_t data) {
  if (data == 0xff) {
    // Mark the current input buffer as ready for display.
    bufidx_ready = bufidx_input;

    // If buffering is enabled, advance to to the next input buffer.
    if (buf_enable) {
      if (++bufidx_input == 3) {
        bufidx_input = 0;  // wrap back to the first buffer
      }
    }
    switch (bufidx_input) {
      case 0:
        base_r6 = D0_R6;
        base_r7 = B1_R7;
        break;
      case 1:
        bufidx_input = 1;
        base_r6 = D0_R6;
        base_r7 = B3_R7;
        break;
      case 2:
        bufidx_input = 2;
        base_r6 = D1_R6;
        base_r7 = B1_R7;
        break;
    }

    // Position MP at the beginning of the selected buffer.
    cur_x = cur_y = 0;
    VIDEO->R6 = base_r6 | 0;  // y
    VIDEO->R7 = base_r7 | 0;  // x
  } else {
    VIDEO->ER1 = data;
    video_wait_busy();

    if (cur_x++ == 39) {
      cur_x = 0;
      VIDEO->R7 = base_r7 | 0;

      if (cur_y++ == 0) {
        cur_y = 8;
      }
      VIDEO->R6 = base_r6 | cur_y;
    }
  }
}

static inline void process_vsync(void) {
  if (bufidx_ready != -1) {
    VIDEO->R1 = RORs[bufidx_ready];
    VIDEO->ER0 = VIDEO_CMD_IND | VIDEO_IND_ROR;
    video_wait_busy();

    // Restore idle command.
    VIDEO->R0 = CMD_IDLE;

    bufidx_ready = -1;
  }
}

static void wait_no_key_pressed(void) {
  while (get_pressed_keys() != 0) {
  }
}

// Connect stdout to video output.
int putchar(int c) {
  VIDEO->R0 = VIDEO_CMD_TSM | VIDEO_MEM_POSTINCR;
  VIDEO->ER2 = c;
  video_wait_busy();

  VIDEO->R0 = CMD_IDLE;
  return c;
}

#define NUM_SPEEDS 8
static const unsigned long speed_baud_rates[NUM_SPEEDS] = {
    1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200,
};
static const uint16_t speed_reload_values[NUM_SPEEDS] = {
    TIMER_TICKS_TO_RELOAD_VALUE_16(TIMER_TICKS_FROM_BAUD_T2(1200)),
    TIMER_TICKS_TO_RELOAD_VALUE_16(TIMER_TICKS_FROM_BAUD_T2(2400)),
    TIMER_TICKS_TO_RELOAD_VALUE_16(TIMER_TICKS_FROM_BAUD_T2(4800)),
    TIMER_TICKS_TO_RELOAD_VALUE_16(TIMER_TICKS_FROM_BAUD_T2(9600)),
    TIMER_TICKS_TO_RELOAD_VALUE_16(TIMER_TICKS_FROM_BAUD_T2(19200)),
    TIMER_TICKS_TO_RELOAD_VALUE_16(TIMER_TICKS_FROM_BAUD_T2(38400)),
    TIMER_TICKS_TO_RELOAD_VALUE_16(TIMER_TICKS_FROM_BAUD_T2(57600)),
    TIMER_TICKS_TO_RELOAD_VALUE_16(TIMER_TICKS_FROM_BAUD_T2(115200)),
};
static uint8_t speed_idx = 3;  // initial choice: 9600

uint16_t speed_selection_menu(void) {
  // Clear buffer #0.
  VIDEO->R1 = 0x07;  // white-on-black
  VIDEO->R2 = ' ';   // blank
  for (uint8_t y = 0; y < 25; y++) {
    VIDEO->R6 = y == 0 ? 0 : (7 + y);
    VIDEO->R7 = 0;
    for (uint8_t x = 0; x < 40; x++) {
      VIDEO->ER0 = VIDEO_CMD_TSM | VIDEO_MEM_POSTINCR;
      video_wait_busy();
    }
  }

  // Switch to buffer #0.
  VIDEO->R1 = 0x08;
  VIDEO->ER0 = VIDEO_CMD_IND | VIDEO_IND_ROR;
  video_wait_busy();

  // Show fixed parts.
  VIDEO->R6 = 27;    // y
  VIDEO->R7 = 0;     // x
  VIDEO->R1 = 0x47;  // black-on-white
  printf(" CONNECT ");
  VIDEO->R1 = 0x07;  // white-on-black
  printf("         Start/stop reception");
  VIDEO->R6 = 29;    // y
  VIDEO->R7 = 0;     // x
  VIDEO->R1 = 0x47;  // black-on-white
  printf(" PREVIOUS ");
  VIDEO->R1 = 0x07;  // white-on-black
  printf(" ");
  VIDEO->R1 = 0x47;  // black-on-white
  printf(" NEXT ");
  VIDEO->R1 = 0x07;  // white-on-black
  printf(" Change baud rate");
  VIDEO->R6 = 31;    // y
  VIDEO->R7 = 0;     // x
  VIDEO->R1 = 0x47;  // black-on-white
  printf(" SPACE ");
  VIDEO->R1 = 0x07;  // white-on-black
  printf("           Toggle buffering");

  // Display loop.
  __bit selection_changed = true;
  while (true) {
    if (selection_changed) {
      selection_changed = false;

      // Move cursor and show currently selected speed (with some extra blanks
      // to cover the previous value, in case it was shorter).
      VIDEO->R6 = 0;  // y
      VIDEO->R7 = 0;  // x
      printf("Baud rate: %lu  ", speed_baud_rates[speed_idx]);
      VIDEO->R6 = 8;  // y
      VIDEO->R7 = 0;  // x
      printf("Buffering: %s ", buf_enable ? "ON" : "OFF");

      // Wait for no key to be pressed before accepting the next input.
      wait_no_key_pressed();
    }

    // Process next input.
    uint8_t key = get_pressed_keys();
    switch (key) {
      case PRESSED_KEY_PREVIOUS:
        if (speed_idx != 0) {
          speed_idx--;
          selection_changed = true;
        }
        break;
      case PRESSED_KEY_NEXT:
        if (speed_idx != NUM_SPEEDS - 1) {
          speed_idx++;
          selection_changed = true;
        }
        break;
      case PRESSED_KEY_SPACE:
        buf_enable = !buf_enable;
        selection_changed = true;
        break;
      case PRESSED_KEY_CONNECT:
        // Wait for the key to be released before returning.
        wait_no_key_pressed();
        return speed_reload_values[speed_idx];
    }
  }
}

void main(void) {
  display_setup();
  board_controls_set_defaults();
#ifdef BOARD_PERIODIC_TASK_HZ
  board_periodic_task_setup();
#endif
  EA = 1;

  // Enable VSYNC bit.
  VIDEO->ER0 = VIDEO_CMD_VRM;
  video_wait_busy();

  while (true) {
    // Show the menu.
    uint16_t rcap2_value = speed_selection_menu();

    // Prefill the first three buffers with attributes that select the mosaic
    // character set.
    display_set_mosaic(D0_R6, B0_R7);
    display_set_mosaic(D0_R6, B2_R7);
    display_set_mosaic(D1_R6, B0_R7);

    // Start the character reception machinery.
    bufidx_input = bufidx_ready = 0;
    VIDEO->R0 = CMD_IDLE;
    process_data(0xFF);

    serial_start(rcap2_value);

    // Execute until CONNECT is pressed again.
    //
    // This loop is very sensible to timing and order of the operations:
    // - Top priority: identifying the VSYNC pulse (which is relatively short).
    // - Normal priority: pushing data to the video RAM.
    // - Low priority: checking the state of the keyboard.
    __bit connect_pressed = false, keyboard_test_scheduled = false;
    while (true) {
      // Detect beginning of VSYNC (falling edge): select the displayed buffer
      // and schedule a check of the keyboard state.
      uint8_t curr_status = VIDEO->R0;
      if ((curr_status & 0x04) == 0 && (prev_status & 0x04) != 0) {
        process_vsync();
        keyboard_test_scheduled = true;
      }
      prev_status = curr_status;

      __bit have_data = false;
      uint8_t data;

      // Pop one byte from the FIFO, unless empty.
      __critical {
        if (fifo_rpos != fifo_wpos) {
          data = fifo[fifo_rpos];
          have_data = true;

          if (++fifo_rpos == sizeof(fifo)) {
            fifo_rpos = 0;
          }
        }
      }

      // Prioritize pushing data to the video RAM. Only execute the keyboard
      // test when the buffer has been fully drained.
      if (have_data) {
        process_data(data);
      } else if (keyboard_test_scheduled) {
        keyboard_test_scheduled = false;

        if (get_pressed_keys() & PRESSED_KEY_CONNECT) {
          break;
        }
      }
    }

    serial_stop();

    // Flush the input buffer.
    fifo_rpos = fifo_wpos = 0;
  }
}
