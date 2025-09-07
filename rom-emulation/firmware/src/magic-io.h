#ifndef ROM_EMULATION_FIRMWARE_SRC_MAGIC_IO_H
#define ROM_EMULATION_FIRMWARE_SRC_MAGIC_IO_H

#include <pico/types.h>
#include <stdint.h>

#include "magic-io-definitions.h"
#include "romemu.h"

// The "magic range" is an area of the emulated ROM, in which the act of reading
// certain locations by the Minitel CPU triggers corresponding actions on our
// Pico side.
constexpr uint16_t MAGIC_RANGE_SIZE = 0x1000;
constexpr uint16_t MAGIC_RANGE_BASE = MAX_ROM_SIZE - MAGIC_RANGE_SIZE;
constexpr uint16_t MAGIC_RANGE_END = MAGIC_RANGE_BASE + sizeof(MAGIC_IO_t);

// The Minitel can send one of these signals to the Pico.
enum class MagicIoSignal {
  None,
  UserRequestedBoot,  // User asked to proceed to the next ROM.
  InTrampoline,       // Readiness to safely switch to another ROM.
};

// If called right after loading a ROM into memory, but before romemu_start(),
// it initializes the in-memory values used by the magic I/O interface.
void magic_io_prepare_rom(MAGIC_IO_DESIRED_STATE_t initial_state);

// Changes the desired state.
void magic_io_set_desired_state(MAGIC_IO_DESIRED_STATE_t new_state);

// Determines what signal is being trasnmitted by the Minitel CPU by looking at
// the most recent ROM accesses.
//
// Note: signals that do not need to be observed from the outside are reported
// as MagicIoSignal::None.
MagicIoSignal magic_io_analyze_traces(const uint16_t *samples,
                                      uint num_samples);

#endif
