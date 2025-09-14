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

static std::optional<uint> configuration_load_last_rom_slot;

void magic_io_prepare_rom(MAGIC_IO_DESIRED_STATE_t initial_state) {
  desired_state = initial_state;

  SET_FIELD(a.user_requested_boot, 1);
  SET_FIELD(p.desired_state, (uint8_t)desired_state);

  SET_FIELD(a.user_requested_client_mode_sync1, 1);
  SET_FIELD(a.user_requested_client_mode_sync2, 0);

  // Initialize serial, emulator-to-minitel direction.
  for (uint i = 0; i < 256; i++) {
    SET_INDEXED_FIELD(a.serial_data_tx, i, 1);
  }
  SET_FIELD(a.serial_data_tx_ack, 0);

  // Initialize serial, minitel-to-emulator direction.
  serial_rx_buf_rpos = serial_rx_buf_cnt = 0;
  SET_FIELD(p.serial_data_rx_nonempty, 0);
  SET_FIELD(a.serial_data_rx_lock, 1);
  SET_FIELD(a.serial_data_rx_unlock, 0);

  // Initialize configuration requests.
  configuration_load_last_rom_slot = std::nullopt;
  for (uint i = 0; i < 16; i++) {
    SET_INDEXED_FIELD(a.configuration_load_block_rom_slot, i, 1);
  }
  SET_FIELD(a.configuration_load_block_ack, 0);

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

void magic_io_enqueue_serial_tx(uint8_t data) {
  if (serial_rx_buf_cnt != sizeof(serial_rx_buf)) {
    uint wpos =
        (serial_rx_buf_rpos + serial_rx_buf_cnt++) % sizeof(serial_rx_buf);
    serial_rx_buf[wpos] = data;
    SET_FIELD(p.serial_data_rx_nonempty, 1);
  }
}

void magic_io_fill_configuration_block(const MAGIC_IO_CONFIGURATION_DATA_t &v) {
  for (uint i = 0; i < sizeof(MAGIC_IO_CONFIGURATION_DATA_t); i++) {
    SET_INDEXED_FIELD(p.configuration_loaded_block, i, v.raw[i]);
  }
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
    case ADDRESS_OF(a.user_requested_boot)...(
        ADDRESS_OF(a.user_requested_boot) + 15): {
      uint8_t slot_num = (uint8_t)(address - ADDRESS_OF(a.user_requested_boot));
      SET_INDEXED_FIELD(a.user_requested_boot, slot_num, 0);
      return (MagicIoSignal)((uint)MagicIoSignal::UserRequestedBoot0 +
                             slot_num);
    }
    case ADDRESS_OF(a.user_requested_client_mode_sync1): {
      SET_FIELD(a.user_requested_client_mode_sync2, 1);
      SET_FIELD(a.user_requested_client_mode_sync1, 0);
      return MagicIoSignal::None;
    }
    case ADDRESS_OF(a.user_requested_client_mode_sync2): {
      SET_FIELD(a.user_requested_client_mode_sync1, 1);
      SET_FIELD(a.user_requested_client_mode_sync2, 0);
      return MagicIoSignal::UserRequestedClientMode;
    }
    case ADDRESS_OF(a.serial_data_tx)...(ADDRESS_OF(a.serial_data_tx) + 0xFF): {
      uint8_t tx_value = (uint8_t)(address - ADDRESS_OF(a.serial_data_tx));
      if (serial_tx_buf != tx_value) {
        // This assignment should never be needed, if the other side is
        // following the protocol, but let's do it out of precaution.
        SET_INDEXED_FIELD(a.serial_data_tx, serial_tx_buf, 1);
        serial_tx_buf = tx_value;
      }
      SET_FIELD(a.serial_data_tx_ack, 1);
      SET_INDEXED_FIELD(a.serial_data_tx, serial_tx_buf, 0);
      return MagicIoSignal::None;
    }
    case ADDRESS_OF(a.serial_data_tx_ack): {
      SET_INDEXED_FIELD(a.serial_data_tx, serial_tx_buf, 1);
      SET_FIELD(a.serial_data_tx_ack, 0);
      return (MagicIoSignal)((uint)MagicIoSignal::SerialRx00 + serial_tx_buf);
    }
    case ADDRESS_OF(a.serial_data_rx_lock): {
      if (serial_rx_buf_cnt == 0) {
        // This should never happen if the other side follows the protocol.
        // Let's try to recover somehow.
        SET_FIELD(p.serial_data_rx_data, '?');
      } else {
        // Pop one element from the circular queue.
        SET_FIELD(p.serial_data_rx_data, serial_rx_buf[serial_rx_buf_rpos++]);
        if (serial_rx_buf_rpos == sizeof(serial_rx_buf)) {
          serial_rx_buf_rpos = 0;
        }
        serial_rx_buf_cnt--;
      }
      SET_FIELD(p.serial_data_rx_nonempty, serial_rx_buf_cnt != 0);
      SET_FIELD(a.serial_data_rx_unlock, 1);
      SET_FIELD(a.serial_data_rx_lock, 0);
      return MagicIoSignal::None;
    }
    case ADDRESS_OF(a.serial_data_rx_unlock): {
      SET_FIELD(a.serial_data_rx_lock, 1);
      SET_FIELD(a.serial_data_rx_unlock, 0);
      return MagicIoSignal::None;
    }
    case ADDRESS_OF(a.configuration_load_block_rom_slot)...(
        ADDRESS_OF(a.configuration_load_block_rom_slot) + 15): {
      uint slot_num =
          (uint)(address - ADDRESS_OF(a.configuration_load_block_rom_slot));
      if (configuration_load_last_rom_slot.has_value()) {
        // This should never happen if the other side follows the protocol.
        // Let's try to recover somehow.
        SET_INDEXED_FIELD(a.configuration_load_block_rom_slot,
                          *configuration_load_last_rom_slot, 1);
      }
      configuration_load_last_rom_slot = slot_num;
      SET_FIELD(a.configuration_load_block_ack, 1);
      SET_INDEXED_FIELD(a.configuration_load_block_rom_slot, slot_num, 0);
      return (MagicIoSignal)((uint)MagicIoSignal::ConfigurationDataRom0 +
                             slot_num);
    }
    case ADDRESS_OF(a.configuration_load_block_ack): {
      if (configuration_load_last_rom_slot.has_value()) {
        SET_INDEXED_FIELD(a.configuration_load_block_rom_slot,
                          *configuration_load_last_rom_slot, 1);
        configuration_load_last_rom_slot = std::nullopt;
      } else {
        // This branch should never be taken if the other side follows the
        // protocol.
      }
      SET_FIELD(a.configuration_load_block_ack, 0);
      return MagicIoSignal::None;
    }
    default: {
      return MagicIoSignal::None;
    }
  }
}
