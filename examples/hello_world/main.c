#include <8052.h>
#include <board/controls.h>
#include <board/definitions.h>
#include <stdbool.h>
#include <stdio.h>
#include <timer/timer.h>

#include "display.h"
#include "keyb.h"
#include "ticks.h"

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

void main(void) {
  // Initialize time tracking, relying on Timer 0 interrupts.
  ticks_setup();

  // Enter 40-character short mode and clear the screen.
  display_setup();
  clear_full();

  // Initialize the board-specific control registers to sensible default values.
  board_controls_set_defaults();
#ifdef BOARD_PERIODIC_TASK_HZ
  board_periodic_task_setup();
#endif

  // Print "Hello World!" in double-height, double-width, flashing, white color.
  draw_string(8, 2, "HHeelllloo  WWoorrlldd!!", 0x3F);
  draw_string(8, 3, "HHeelllloo  WWoorrlldd!!", 0x3F);

  // Print color matrix.
  draw_color_pattern(0, 5);

  // Print skeleton for keyboard matrix.
  keyb_widget_prepare(21, 5);

  // More example texts showcasing text sizing.
  draw_string(0, 21, "Normal Size", 0x07);
  draw_string(16, 21, "DDoouubbllee  Wwiiddtthh", 0x27);
  draw_string(0, 23, "Double Height", 0x17);
  draw_string(0, 24, "Double Height", 0x17);
  draw_string(16, 23, "DDoouubbllee  WW  &&  HH", 0x37);
  draw_string(16, 24, "DDoouubbllee  WW  &&  HH", 0x37);

  // Enable interrupts globally. This starts delivering the Timer 0 interrupt
  // that was previously configured by ticks_setup() as well as Timer 1 for the
  // board periodic task.
  EA = 1;

  while (true) {
    // Print the number of milliseconds since boot.
    char buf[11];
    int ndigits = sprintf(buf, "%lu", ticks_get());
    draw_string(0, 0, buf, 0x07);
    draw_string(ndigits, 0, " ms", 0x07);

    // Print the current keyboard state.
    keyb_widget_fill(21, 5);
  }
}
