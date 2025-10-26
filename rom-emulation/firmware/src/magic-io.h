#ifndef ROM_EMULATION_FIRMWARE_SRC_MAGIC_IO_H
#define ROM_EMULATION_FIRMWARE_SRC_MAGIC_IO_H

#include <pico/types.h>
#include <stdint.h>

#include "magic-io-definitions.h"
#include "mememu.h"

// The "magic range" is an area of the emulated ROM, in which the act of reading
// certain locations by the Minitel CPU triggers corresponding actions on our
// Pico side.
constexpr uint16_t MAGIC_RANGE_SIZE = 0x1000;
constexpr uint16_t MAGIC_RANGE_BASE = MAX_MEM_SIZE - MAGIC_RANGE_SIZE;
constexpr uint16_t MAGIC_RANGE_END = MAGIC_RANGE_BASE + sizeof(MAGIC_IO_t);

// The Minitel can send one of these signals to the Pico.
enum class MagicIoSignal {
  None,
  UserRequestedClientMode,  // User asked to start serial tunnel.
  InTrampoline,             // Readiness to safely switch to another ROM.

  // User asked to proceed to the ROM in the given slot number.
  UserRequestedBoot0,
  // ... all the possible values in between ...
  UserRequestedBoot15 = UserRequestedBoot0 + 15,

  // Request to populate the given configuration block with current data.
  ConfigurationDataRom0,
  // ... all the possible values in between ...
  ConfigurationDataRom15 = ConfigurationDataRom0 + 15,
  ConfigurationDataNetwork,

  // Serial data received by the Minitel CPU and forwarded to the Pico.
  SerialRx00,
  // ... all the possible values in between ...
  SerialRxFF = SerialRx00 + 0xFF,
};

// If called right after loading a ROM into memory, but before mememu_start(),
// it initializes the in-memory values used by the magic I/O interface.
void magic_io_prepare_rom(MAGIC_IO_DESIRED_STATE_t initial_state);

// Changes the desired state.
void magic_io_set_desired_state(MAGIC_IO_DESIRED_STATE_t new_state);

// Enqueues a byte so that it will eventually be emitted by the Minitel's CPU.
void magic_io_enqueue_serial_tx(uint8_t data);

// Fill the configuration block with the given data. This function must be
// called in response to ConfigurationData* signals.
void magic_io_fill_configuration_block(const MAGIC_IO_CONFIGURATION_DATA_t &v);

// Signal that at least one configuration block has changed.
void magic_io_signal_configuration_changed();

// Determines what signal is being transmitted by the Minitel CPU by looking at
// the most recent ROM accesses.
//
// Note: signals that do not need to be observed from the outside are reported
// as MagicIoSignal::None.
MagicIoSignal magic_io_analyze_traces(const uint16_t *samples,
                                      uint num_samples);

#endif
