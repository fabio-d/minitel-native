#ifndef ROM_EMULATION_FIRMWARE_MENU_MAGIC_IO_H
#define ROM_EMULATION_FIRMWARE_MENU_MAGIC_IO_H

#include "magic-io-definitions.h"

void magic_io_reset(void);

void magic_io_signal_user_requested_boot(void);

MAGIC_IO_DESIRED_STATE_t magic_io_get_desired_state(void);

void magic_io_jump_to_trampoline(void);

#endif
