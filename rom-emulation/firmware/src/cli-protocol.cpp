#include "cli-protocol.h"

#include <assert.h>

static constexpr uint8_t MAGIC_BEGIN_1 = 0xA7;
static constexpr uint8_t MAGIC_BEGIN_2 = 0x5C;
static constexpr uint8_t MAGIC_END_1 = 0xE1;
static constexpr uint8_t MAGIC_END_2 = 0x6D;

static uint16_t crc16_step(uint8_t value, uint16_t crc) {
  crc ^= (uint16_t)value << 8;
  for (int i = 0; i < 8; i++) {
    bool do_xor = (crc & 0x8000) != 0;
    crc = crc << 1;
    if (do_xor) {
      crc ^= 0x1021;
    }
  }
  return crc;
}

CliProtocolDecoder::CliProtocolDecoder() { reset(); }

void CliProtocolDecoder::reset() { state = State::WaitForMagicBegin1; }

CliProtocolDecoder::PushResult CliProtocolDecoder::push(uint8_t byte) {
  switch (state) {
    case State::WaitForMagicBegin1:  // about to receive the initial packet.
    case State::PacketAvailable:     // about to receive a non-initial packet.
      state = byte == MAGIC_BEGIN_1 ? State::WaitForMagicBegin2 : State::Error;
      break;
    case State::WaitForMagicBegin2:
      state = byte == MAGIC_BEGIN_2 ? State::WaitForLength1 : State::Error;
      break;
    case State::WaitForLength1:
      length = byte;
      crc = crc16_step(byte, 0);
      state = State::WaitForLength2;
      break;
    case State::WaitForLength2:
      length |= (uint16_t)byte << 8;
      crc = crc16_step(byte, crc);
      // Reject packets that would not fit in the buffer.
      state = length <= sizeof(buf) ? State::WaitForPacketType : State::Error;
      break;
    case State::WaitForPacketType:
      packet_type = byte;
      pos = 0;
      crc = crc16_step(byte, crc);
      state = length != 0 ? State::WaitForData : State::WaitForChecksum1;
      break;
    case State::WaitForData:
      buf[pos++] = byte;
      crc = crc16_step(byte, crc);
      if (pos == length) {
        state = State::WaitForChecksum1;
      }
      break;
    case State::WaitForChecksum1:
      state = (crc & 0xFF) == byte ? State::WaitForChecksum2 : State::Error;
      break;
    case State::WaitForChecksum2:
      state = (crc >> 8) == byte ? State::WaitForMagicEnd1 : State::Error;
      break;
    case State::WaitForMagicEnd1:
      state = byte == MAGIC_END_1 ? State::WaitForMagicEnd2 : State::Error;
      break;
    case State::WaitForMagicEnd2:
      if (byte == MAGIC_END_2) {
        state = State::PacketAvailable;
      } else {
        state = State::Error;
      }
      break;
    case State::Error:
      break;
  }

  switch (state) {
    case State::Error:
      return PushResult::Error;
    case State::PacketAvailable:
      return PushResult::PacketAvailable;
    default:
      return PushResult::Idle;
  }
}

uint8_t CliProtocolDecoder::get_packet_type() const {
  assert(state == State::PacketAvailable);
  return packet_type;
}

const void *CliProtocolDecoder::get_packet_data() const {
  assert(state == State::PacketAvailable);
  return buf;
}

uint CliProtocolDecoder::get_packet_length() const {
  assert(state == State::PacketAvailable);
  return length;
}

CliProtocolEncoder::CliProtocolEncoder() {
  length = INVALID_LENGTH;

  // These positions are constant and they can be prefilled.
  buf[0] = MAGIC_BEGIN_1;
  buf[1] = MAGIC_BEGIN_2;
}

void CliProtocolEncoder::begin(uint8_t packet_type) {
  buf[4] = packet_type;
  length = 0;
}

void CliProtocolEncoder::push(uint8_t byte) {
  assert(length != INVALID_LENGTH);
  assert(length < MAX_DATA_LENGTH);
  buf[5 + length++] = byte;
}

void CliProtocolEncoder::push(const void *data, uint len) {
  const uint8_t *ptr = (const uint8_t *)data;
  while (len-- != 0) {
    push(*ptr++);
  }
}

std::pair<const uint8_t *, uint> CliProtocolEncoder::finalize() {
  assert(length != INVALID_LENGTH);

  // Fill length in the header.
  buf[2] = length & 0xFF;
  buf[3] = length >> 8;

  // Compute the checksum.
  uint16_t crc = 0;
  for (uint i = 2; i < 5 + length; i++) {
    crc = crc16_step(buf[i], crc);
  }

  // Append footer.
  uint wpos = 5 + length;
  buf[wpos++] = crc & 0xFF;
  buf[wpos++] = crc >> 8;
  buf[wpos++] = MAGIC_END_1;
  buf[wpos++] = MAGIC_END_2;

  return {buf, wpos};
}
