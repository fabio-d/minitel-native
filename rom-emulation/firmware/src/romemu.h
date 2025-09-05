#ifndef ROM_EMULATION_FIRMWARE_SRC_ROMEMU_H
#define ROM_EMULATION_FIRMWARE_SRC_ROMEMU_H

#include <stddef.h>
#include <stdint.h>

constexpr size_t MAX_ROM_SIZE = 0x10000;

// Initializes the GPIOs and PIO machines and starts responding with a fixed
// value of 0x00 regardless of the requested address.
void romemu_setup();

// Starts responding with real data (that must have been previously filled with
// romemu_write).
void romemu_start();

// Sets one byte of the emulated ROM.
void romemu_write(uint16_t address, uint8_t value);

#endif
