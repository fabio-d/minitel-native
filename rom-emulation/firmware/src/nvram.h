#ifndef ROM_EMULATION_FIRMWARE_SRC_NVRAM_H
#define ROM_EMULATION_FIRMWARE_SRC_NVRAM_H

#include <pico/stdlib.h>
#include <stddef.h>
#include <stdint.h>

// Initializes the driver for the 23LCV512 (connected over SPI) and leaves it
// in a state suitable for calling all the functions except nvram_write_fast.
void nvram_setup();

// Writes one byte into the NVRAM.
void nvram_write(uint16_t addr, uint8_t data);

// Reads one byte from the NVRAM.
uint8_t nvram_read(uint16_t addr);

// Reads several consecutive bytes from the NVRAM.
void nvram_read_burst(uint16_t addr, size_t n_bytes,
                      void (*cb)(uint16_t addr, uint8_t data));

// Enables the usage of nvram_write_fast. From this moment on, no other function
// from this driver other than nvram_write_fast can be called.
void nvram_enter_write_fast_mode();

// Unlike nvram_write, this function does not wait completion. It is up to the
// caller to avoid calling it too often, or data will be lost.
void __scratch_x("nvram_write_fast")
    nvram_write_fast(uint16_t addr, uint8_t data);

#endif
