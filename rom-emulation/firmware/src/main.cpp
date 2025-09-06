#include <hardware/structs/busctrl.h>
#include <pico/binary_info.h>
#include <pico/stdlib.h>
#include <pico/time.h>
#include <stdio.h>
#include <stdlib.h>

#include "cli-protocol.h"
#include "embedded-rom-array.h"
#include "led.h"
#include "romemu.h"
#include "trace.h"

bi_decl(bi_program_feature(MINITEL_MODEL_FEATURE));
bi_decl(bi_program_feature(OPERATING_MODE_FEATURE));

static_assert(sizeof(EMBEDDED_ROM) <= MAX_ROM_SIZE);

constexpr uint TRACE_MAX_SAMPLES = 128;
static uint16_t trace_buf[TRACE_MAX_SAMPLES];

static CliProtocolDecoder stdio_decoder;
static CliProtocolEncoder encoder;

static std::pair<const uint8_t *, uint> handle_packet(uint8_t packet_type,
                                                      const void *packet_data,
                                                      uint packet_length) {
  switch (packet_type) {
    case CLI_PACKET_TYPE_EMULATOR_PING: {
      encoder.begin(CLI_PACKET_TYPE_EMULATOR_PING ^
                    CLI_PACKET_TYPE_REPLY_XOR_MASK);
      encoder.push(packet_data, packet_length);  // echo back the same data
      return encoder.finalize();
    }
    case CLI_PACKET_TYPE_EMULATOR_TRACE: {
      uint num_samples = trace_collect(TRACE_MAX_SAMPLES,
                                       make_timeout_time_us(150), trace_buf);
      encoder.begin(CLI_PACKET_TYPE_EMULATOR_TRACE ^
                    CLI_PACKET_TYPE_REPLY_XOR_MASK);
      encoder.push(trace_buf, num_samples * sizeof(uint16_t));
      return encoder.finalize();
    }
    default: {  // Unknown packet_type.
      return {0, 0};
    }
  }
}

int main() {
  // Give core1 (the CPU that will serve the ROM) priority access to the RAM,
  // so that it's never stalled.
  busctrl_hw->priority = BUSCTRL_BUS_PRIORITY_PROC1_BITS;

  // Take over the duty of responding to PSEN requests from the SN74HCT541 to
  // ourselves.
  romemu_setup();

  // Start recording requested ROM addresses. Since nothing is consuming them,
  // the FIFO will overflow and stay that way until trace_collect is called.
  trace_setup();

  // Initialize the status LED.
  led_setup();
  led_set(true);

  // Fill the emulated ROM.
  for (size_t i = 0; i < sizeof(EMBEDDED_ROM); i++) {
    romemu_write(i, EMBEDDED_ROM[i]);
  }

  stdio_init_all();
  sleep_ms(1000);

  romemu_start();

  absolute_time_t next_toggle = get_absolute_time();
  bool led_on = true;
  while (1) {
    // Blink at 1 Hz.
    absolute_time_t now = get_absolute_time();
    if (absolute_time_diff_us(next_toggle, now) >= 0) {
      next_toggle = delayed_by_ms(next_toggle, 500);
      led_on = !led_on;
      led_set(led_on);
    }

    // Interpret bytes received over USB with the client protocol.
    uint32_t r = stdio_getchar_timeout_us(0);
    if (r != PICO_ERROR_TIMEOUT) {
      switch (stdio_decoder.push(r)) {
        case CliProtocolDecoder::PushResult::Idle: {
          break;
        }
        case CliProtocolDecoder::PushResult::Error: {
          stdio_decoder.reset();
          break;
        }
        case CliProtocolDecoder::PushResult::PacketAvailable: {
          auto [reply_data, reply_length] = handle_packet(
              stdio_decoder.get_packet_type(), stdio_decoder.get_packet_data(),
              stdio_decoder.get_packet_length());
          for (uint i = 0; i < reply_length; i++) {
            stdio_putchar_raw(reply_data[i]);
          }
          break;
        }
      }
    }
  }
}
