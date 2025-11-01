#include "magic-io.h"

#include <8052.h>
#include <stdbool.h>
#include <stdio.h>

#define MAGIC_IO ((__code MAGIC_IO_t *)0xF000)

void magic_io_reset(void) {
  uint8_t initial_value = MAGIC_IO->a.reset_generation_count;
  while (MAGIC_IO->a.reset_generation_count == initial_value) {
  }
}

void magic_io_signal_user_requested_boot(uint8_t slot_num) {
  while (MAGIC_IO->a.user_requested_boot[slot_num]) {
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
  // Set registers back to their reset values and jump to the fixed address
  // where the trampoline is.

  IE = 0x00;
  SCON = 0x00;
  TCON = 0x00;
  T2CON = 0x00;
  RCAP2L = 0x00;
  RCAP2H = 0x00;
  TL2 = 0x00;
  TH2 = 0x00;
  P0 = 0xFF;
  P1 = 0xFF;
  P2 = 0xFF;
  P3 = 0xFF;

  __asm__(
      // Clear register bank #3.
      "mov psw, #(3 << 3)\n"
      "mov r0, #0x00\n"
      "mov r1, #0x00\n"
      "mov r2, #0x00\n"
      "mov r3, #0x00\n"
      "mov r4, #0x00\n"
      "mov r5, #0x00\n"
      "mov r6, #0x00\n"
      "mov r7, #0x00\n"

      // Clear register bank #2.
      "mov psw, #(2 << 3)\n"
      "mov r0, #0x00\n"
      "mov r1, #0x00\n"
      "mov r2, #0x00\n"
      "mov r3, #0x00\n"
      "mov r4, #0x00\n"
      "mov r5, #0x00\n"
      "mov r6, #0x00\n"
      "mov r7, #0x00\n"

      // Clear register bank #1.
      "mov psw, #(1 << 3)\n"
      "mov r0, #0x00\n"
      "mov r1, #0x00\n"
      "mov r2, #0x00\n"
      "mov r3, #0x00\n"
      "mov r4, #0x00\n"
      "mov r5, #0x00\n"
      "mov r6, #0x00\n"
      "mov r7, #0x00\n"

      // Clear register bank #0 and leave it selected.
      "mov psw, #(0 << 3)\n"
      "mov r0, #0x00\n"
      "mov r1, #0x00\n"
      "mov r2, #0x00\n"
      "mov r3, #0x00\n"
      "mov r4, #0x00\n"
      "mov r5, #0x00\n"
      "mov r6, #0x00\n"
      "mov r7, #0x00\n"

      // Clear other registers.
      "mov sp, #0x07\n"
      "mov a, #0x00\n"
      "mov b, #0x00\n"
      "mov dph, #0x00\n"
      "mov dpl, #0x00\n"

      "ljmp 0xFFFD\n");
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

bool magic_io_test_and_clear_configuration_changed(void) {
  if (!MAGIC_IO->a.configuration_changed) {
    return false;
  }

  while (MAGIC_IO->a.configuration_changed) {
  }
  return true;
}

__code const MAGIC_IO_CONFIGURATION_DATA_ROM_t *
magic_io_get_configuration_rom_slot(uint8_t slot_num) {
  while (MAGIC_IO->a.configuration_load_block_rom_slot[slot_num]) {
  }
  while (MAGIC_IO->a.configuration_load_block_ack) {
  }

  return &MAGIC_IO->p.configuration_loaded_block.rom;
}

__code const MAGIC_IO_CONFIGURATION_DATA_NETWORK_t *
magic_io_get_configuration_network(void) {
  while (MAGIC_IO->a.configuration_load_block_network) {
  }
  while (MAGIC_IO->a.configuration_load_block_ack) {
  }

  return &MAGIC_IO->p.configuration_loaded_block.network;
}
