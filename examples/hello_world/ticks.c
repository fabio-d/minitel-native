#include "ticks.h"

#include <stdbool.h>

#include "timer/timer.h"

static volatile unsigned long ticks = 0;

static inline void timer0_reload() {
  const uint16_t reload_value =
      TIMER_TICKS_TO_RELOAD_VALUE_16(TIMER_TICKS_FROM_US(1000));
  TH0 = reload_value >> 8;
  TL0 = reload_value;
}

void ticks_interrupt(void) __interrupt(TF0_VECTOR) {
  timer0_reload();
  ticks++;
}

void ticks_setup(void) {
  timer0_reload();

  // Set Timer0 in mode 1 and start it.
  TMOD = (TMOD & 0xf0) | 1;
  TR0 = 1;

  // Enable Timer0 interrupt.
  ET0 = 1;
}

unsigned long ticks_get() {
  unsigned long result;

  // Reading the current value takes more than one instruction. Let's disable
  // interrupts while reading it to make the read atomic.
  __critical { result = ticks; }

  return result;
}
