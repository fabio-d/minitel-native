#include "magic-io.h"

#include <pico/stdlib.h>

#include <optional>

#include "cli-protocol.h"

// This 2-byte area at the end of the ROM's address space contains an infinite
// loop that, once entered by the Minitel CPU, triggers the process of switching
// to a different ROM.
constexpr uint16_t TRAMPOLINE_ADDRESS = 0xFFFD;

static_assert(MAGIC_RANGE_BASE + sizeof(MAGIC_IO_t) <= TRAMPOLINE_ADDRESS,
              "MAGIC_IO_t does not fit in the reserved portion of the ROM");

// Helper macros for manipulating the magic range.
#define ADDRESS_OF(field_name) \
  (MAGIC_RANGE_BASE + offsetof(MAGIC_IO_t, field_name))
#define SET_FIELD(field_name, new_value) \
  romemu_write(ADDRESS_OF(field_name), new_value)
#define SET_INDEXED_FIELD(field_name, index, new_value) \
  romemu_write(ADDRESS_OF(field_name) + index, new_value)

static uint8_t reset_generation_count = 0;
static MAGIC_IO_DESIRED_STATE_t desired_state;

static uint8_t serial_tx_buf = 0;

static uint8_t serial_rx_buf[CLI_PACKET_MAX_ENCODED_LENGTH];
static uint serial_rx_buf_rpos = 0, serial_rx_buf_cnt = 0;

void magic_io_prepare_rom(MAGIC_IO_DESIRED_STATE_t initial_state) {
  desired_state = initial_state;

  SET_FIELD(a.user_requested_boot, 1);
  SET_FIELD(p.desired_state, (uint8_t)desired_state);

  // Write trampoline (infinite SJMP loop followed by a NOP).
  romemu_write(TRAMPOLINE_ADDRESS + 0, 0x80);
  romemu_write(TRAMPOLINE_ADDRESS + 1, 0xFE);
  romemu_write(TRAMPOLINE_ADDRESS + 2, 0x00);

  // This must be done last because it is also used for signalling in response
  // to reset requests.
  SET_FIELD(a.reset_generation_count, ++reset_generation_count);
}

void magic_io_set_desired_state(MAGIC_IO_DESIRED_STATE_t new_state) {
  desired_state = new_state;
  SET_FIELD(p.desired_state, (uint8_t)desired_state);
}

MagicIoSignal magic_io_analyze_traces(const uint16_t *samples,
                                      uint num_samples) {
  // Where all the accesses in the trampoline area?
  bool in_trampoline = true;
  for (uint i = 0; i < num_samples; i++) {
    if (samples[i] < TRAMPOLINE_ADDRESS) {
      in_trampoline = false;
    }
  }
  if (in_trampoline) {
    // Fill the whole ROM with NOPs.
    for (uint16_t i = 0; i < TRAMPOLINE_ADDRESS; i++) {
      romemu_write(i, 0x00);
    }
    romemu_write(TRAMPOLINE_ADDRESS + 1, 0x00);  // override jmp target first.
    sleep_us(200);
    romemu_write(TRAMPOLINE_ADDRESS + 0, 0x00);
    sleep_us(200);
    return MagicIoSignal::InTrampoline;
  }

  // Was there a clear single accessed address in the magic range?
  uint16_t address;
  uint num_hits = 0;
  for (uint i = 0; i < num_samples; i++) {
    if (MAGIC_RANGE_BASE <= samples[i] &&
        samples[i] < MAGIC_RANGE_BASE + sizeof(MAGIC_IO_t::ACTIVE_AREA)) {
      if (num_hits != 0 && address != samples[i]) {
        // If we had already seen a different one, which one to act on would be
        // unclear. It's better to play safe and do nothing, as the program
        // running on the Minitel will simply retry.
        return MagicIoSignal::None;
      }

      address = samples[i];
      num_hits++;
    }
  }

  // Only act if the address has been accessed at least 3 times in the trace, to
  // discard false matches due to, for instance, uncontrolled execution or
  // execution of the NOP slide.
  if (num_hits < 3) {
    return MagicIoSignal::None;
  }

  switch (address) {
    case ADDRESS_OF(a.reset_generation_count): {
      // This is a reset request: let's reinitialize the state of all the
      // protocols. As the last operation, magic_io_prepare_rom will also
      // ack the request by incrementing the value at this address.
      magic_io_prepare_rom(desired_state);
      return MagicIoSignal::None;
    }
    case ADDRESS_OF(a.user_requested_boot): {
      SET_FIELD(a.user_requested_boot, 0);
      return MagicIoSignal::UserRequestedBoot;
    }
    default: {
      return MagicIoSignal::None;
    }
  }
}
