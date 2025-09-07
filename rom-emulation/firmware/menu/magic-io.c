#include "magic-io.h"

#include <stdbool.h>
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

void magic_io_signal_user_client_mode(void) {
  while (MAGIC_IO->a.user_requested_client_mode_sync1) {
  }
  while (MAGIC_IO->a.user_requested_client_mode_sync2) {
  }
}

MAGIC_IO_DESIRED_STATE_t magic_io_get_desired_state(void) {
  return MAGIC_IO->p.desired_state;
}

void magic_io_jump_to_trampoline(void) {
  // Jump to the fixed address where the trampoline is.
  __asm__("ljmp 0xFFFD");
}

void magic_io_tx_byte(uint8_t c) {
  while (MAGIC_IO->a.serial_data_tx[c]) {
  }
  while (MAGIC_IO->a.serial_data_tx_ack) {
  }
}

bool magic_io_rx_byte(uint8_t *c) {
  if (!MAGIC_IO->p.serial_data_rx_nonempty) {
    return false;
  }

  while (MAGIC_IO->a.serial_data_rx_lock) {
  }

  *c = MAGIC_IO->p.serial_data_rx_data;

  while (MAGIC_IO->a.serial_data_rx_unlock) {
  }

  return true;
}
