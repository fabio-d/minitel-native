#ifndef ROM_EMULATION_FIRMWARE_SRC_MEMEMU_H
#define ROM_EMULATION_FIRMWARE_SRC_MEMEMU_H

#include <stddef.h>
#include <stdint.h>

constexpr size_t MAX_MEM_SIZE = 0x10000;

// Initializes the GPIOs and PIO machines and starts responding with a fixed
// value of 0x00 regardless of the requested address.
void mememu_setup();

// Starts responding with real data (that must have been previously filled with
// mememu_write_rom).
void mememu_start();

// Stops responding with real data and starts responding with 0x00 again.
void mememu_stop();

// Sets one byte of the emulated ROM.
void mememu_write_rom(uint16_t address, uint8_t value);

// Sets one byte of the emulated RAM.
void mememu_write_ram(uint16_t address, uint8_t value);

#endif
