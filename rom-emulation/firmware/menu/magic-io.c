#include "magic-io.h"

#include <stdio.h>

#define MAGIC_IO ((__code MAGIC_IO_t *)0xF000)

void magic_io_reset(void) {
  uint8_t initial_value = MAGIC_IO->a.reset_generation_count;
  while (MAGIC_IO->a.reset_generation_count == initial_value) {
  }
}

void magic_io_signal_user_requested_boot(void) {
  while (MAGIC_IO->a.user_requested_boot) {
  }
}

MAGIC_IO_DESIRED_STATE_t magic_io_get_desired_state(void) {
  return MAGIC_IO->p.desired_state;
}

void magic_io_jump_to_trampoline(void) {
  // Jump to the fixed address where the trampoline is.
  __asm__("ljmp 0xFFFD");
}
