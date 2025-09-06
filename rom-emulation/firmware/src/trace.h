#ifndef ROM_EMULATION_FIRMWARE_SRC_TRACE_H
#define ROM_EMULATION_FIRMWARE_SRC_TRACE_H

#include <pico/types.h>
#include <stdint.h>

// Starts the PIO machine that captures the address of every access to program
// memory.
void trace_setup();

// Collects addresses until either the given number of samples if reached, or
// the deadline expires. Returns the number of collected samples.
uint trace_collect(uint max_samples, absolute_time_t deadline, uint16_t *buf);

#endif
