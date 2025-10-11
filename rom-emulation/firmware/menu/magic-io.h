#ifndef ROM_EMULATION_FIRMWARE_MENU_MAGIC_IO_H
#define ROM_EMULATION_FIRMWARE_MENU_MAGIC_IO_H

#include "magic-io-definitions.h"

void magic_io_reset(void);

void magic_io_signal_user_requested_boot(uint8_t slot_num);

void magic_io_signal_user_client_mode(void);

MAGIC_IO_DESIRED_STATE_t magic_io_get_desired_state(void);

void magic_io_jump_to_trampoline(void);

void magic_io_tx_byte(uint8_t c);

bool magic_io_rx_byte(uint8_t* c);

bool magic_io_test_and_clear_configuration_changed(void);

__code const MAGIC_IO_CONFIGURATION_DATA_ROM_t*
magic_io_get_configuration_rom_slot(uint8_t slot_num);

__code const MAGIC_IO_CONFIGURATION_DATA_NETWORK_t*
magic_io_get_configuration_network(void);

#endif
