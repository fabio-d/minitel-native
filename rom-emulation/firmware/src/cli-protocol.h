#ifndef ROM_EMULATION_FIRMWARE_SRC_CLI_PROTOCOL_H
#define ROM_EMULATION_FIRMWARE_SRC_CLI_PROTOCOL_H

#include <pico/types.h>

#include <utility>

constexpr uint16_t CLI_PROTOCOL_TCP_PORT = 3759;

constexpr uint8_t CLI_PACKET_TYPE_EMULATOR_PING = 0;
constexpr uint8_t CLI_PACKET_TYPE_EMULATOR_TRACE = 1;
constexpr uint8_t CLI_PACKET_TYPE_EMULATOR_BOOT = 2;
constexpr uint8_t CLI_PACKET_TYPE_EMULATOR_WRITE_BEGIN = 3;
constexpr uint8_t CLI_PACKET_TYPE_EMULATOR_WRITE_DATA = 4;
constexpr uint8_t CLI_PACKET_TYPE_EMULATOR_WRITE_END = 5;
constexpr uint8_t CLI_PACKET_TYPE_EMULATOR_WIRELESS_CONFIG = 6;
constexpr uint8_t CLI_PACKET_TYPE_EMULATOR_OTA_BEGIN = 7;
constexpr uint8_t CLI_PACKET_TYPE_EMULATOR_OTA_DATA = 8;
constexpr uint8_t CLI_PACKET_TYPE_EMULATOR_OTA_END = 9;
constexpr uint8_t CLI_PACKET_TYPE_REPLY_XOR_MASK = 0x80;

constexpr uint CLI_PACKET_MAX_DATA_LENGTH = 1024;
constexpr uint CLI_PACKET_MAX_ENCODED_LENGTH =
    2 + 2 + 1 + CLI_PACKET_MAX_DATA_LENGTH + 2 + 2;

class CliProtocolDecoder {
 public:
  enum class PushResult {
    Idle,
    Error,
    PacketAvailable,
  };

  CliProtocolDecoder();

  // Clears the current Error or PacketAvailable state.
  void reset();

  // Ingests the next byte of encoded data, returning the new resulting state.
  PushResult push(uint8_t byte);

  // Retrieves the decoded packet, to be called immediately after push()
  // returned PushResult::PacketAvailable.
  uint8_t get_packet_type() const;
  const void *get_packet_data() const;
  uint get_packet_length() const;

 private:
  enum class State {
    WaitForMagicBegin1,
    WaitForMagicBegin2,
    WaitForLength1,
    WaitForLength2,
    WaitForPacketType,
    WaitForData,
    WaitForChecksum1,
    WaitForChecksum2,
    WaitForMagicEnd1,
    WaitForMagicEnd2,
    Error,
    PacketAvailable,
  } state;

  uint8_t packet_type;
  uint length;
  uint pos;
  uint16_t crc;
  alignas(max_align_t) uint8_t buf[CLI_PACKET_MAX_DATA_LENGTH];
};

class CliProtocolEncoder {
 public:
  CliProtocolEncoder();

  // Starts a new packet.
  void begin(uint8_t packet_type);

  // Appends data to the packet being built.
  void push(uint8_t byte);
  void push(const void *data, uint len);

  // Finalizes the packet and retrieves its encoded representation.
  //
  // The returned data stays valid until the next begin() call.
  std::pair<const uint8_t *, uint> finalize();

 private:
  static constexpr uint INVALID_LENGTH = -1;
  uint8_t buf[2 + 2 + 1 + CLI_PACKET_MAX_DATA_LENGTH + 2 + 2];
  uint length;  // INVALID_LENGTH = packet not started yet
};

#endif
