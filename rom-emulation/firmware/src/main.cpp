#include <hardware/structs/busctrl.h>
#include <pico/binary_info.h>
#include <pico/stdlib.h>
#include <pico/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if ROM_EMULATOR_WITH_WIRELESS == 1
#include <lwip/tcp.h>
#include <pico/cyw43_arch.h>
#endif

#include <algorithm>
#include <memory>

#include "cli-protocol.h"
#include "embedded-rom-array.h"
#include "led.h"
#include "magic-io.h"
#include "partition.h"
#include "romemu.h"
#include "trace.h"

bi_decl(bi_program_feature(MINITEL_MODEL_FEATURE));

#if ROM_EMULATOR_IS_INTERACTIVE == 0
bi_decl(bi_program_feature("Embedded operating mode"));
static_assert(sizeof(EMBEDDED_ROM) <= MAX_ROM_SIZE);
#elif ROM_EMULATOR_IS_INTERACTIVE == 1
bi_decl(bi_program_feature("Interactive operating mode"));
static_assert(sizeof(EMBEDDED_ROM) <= MAGIC_RANGE_BASE);
#else
#error Missing or invalid ROM_EMULATOR_IS_INTERACTIVE macro
#endif

static bool in_menu = false;
static bool can_accept_boot_command = false;

constexpr uint TRACE_MAX_SAMPLES = 128;
static uint16_t trace_buf[TRACE_MAX_SAMPLES];

static CliProtocolDecoder magic_io_decoder;
static CliProtocolDecoder stdio_decoder;
static CliProtocolDecoder tcp_decoder;
static CliProtocolEncoder encoder;

static ConfigurationPartition data_partition;
static uint selected_boot_slot_num;

#if ROM_EMULATOR_WITH_WIRELESS == 1
static void on_status_changed(netif *) {
  if (in_menu) {
    // Refresh the menu, to show the new IP address.
    magic_io_signal_configuration_changed();
  }
}

static void reload_wireless_config() {
  // Stop the previous connection.
  cyw43_arch_disable_sta_mode();
  on_status_changed(&netif_list[0]);

  const ConfigurationPartition::WirelessConfig &cfg =
      data_partition.get_wireless_config();
  if (!cfg.is_configured()) {
    return;  // Nothing to do.
  }

  // Start the new connection.
  cyw43_arch_enable_sta_mode();
  netif_set_status_callback(&netif_list[0], on_status_changed);
  netif_set_link_callback(&netif_list[0], on_status_changed);
  if (cfg.is_open()) {
    cyw43_arch_wifi_connect_async(cfg.ssid, nullptr, CYW43_AUTH_OPEN);
  } else {
    cyw43_arch_wifi_connect_async(cfg.ssid, cfg.psk, CYW43_AUTH_WPA2_AES_PSK);
  }
}
#endif

// Write operations are stateful. In order to block interlacing of distinct
// writes from different sources, which would end up reciprocally corrupting
// their states, we only allow the most recently started write operation to
// continue.
static enum class PacketSource {
  Uninitialized,
  MagicIo,
  Stdio,
  TcpClient,
} write_token = PacketSource::Uninitialized;

static std::pair<const uint8_t *, uint> handle_packet(
    uint8_t packet_type, const void *packet_data, uint packet_length,
    PacketSource packet_source) {
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
    case CLI_PACKET_TYPE_EMULATOR_BOOT: {
      if (packet_length != 1 || *(uint8_t *)packet_data >= 16) {
        return {nullptr, 0};  // Malformed request: do not reply.
      }

      encoder.begin(CLI_PACKET_TYPE_EMULATOR_BOOT ^
                    CLI_PACKET_TYPE_REPLY_XOR_MASK);
      selected_boot_slot_num = *(const uint8_t *)packet_data;
      bool slot_is_present =
          data_partition.get_rom_info(selected_boot_slot_num).is_present();
      if (can_accept_boot_command) {
        if (slot_is_present) {
          magic_io_set_desired_state(MAGIC_IO_DESIRED_STATE_BOOT_TRAMPOLINE);
          encoder.push("OK", 2);
        } else {
          magic_io_set_desired_state(MAGIC_IO_DESIRED_STATE_EMPTY_SLOT_ERROR);
          encoder.push("EMPTY", 5);
        }
        can_accept_boot_command = false;
      } else {
        encoder.push("BUSY", 4);
      }
      return encoder.finalize();
    }
    case CLI_PACKET_TYPE_EMULATOR_WRITE_BEGIN: {
      if (packet_length < 2 || *(uint8_t *)packet_data >= 16) {
        return {nullptr, 0};  // Malformed request: do not reply.
      }

      encoder.begin(CLI_PACKET_TYPE_EMULATOR_WRITE_BEGIN ^
                    CLI_PACKET_TYPE_REPLY_XOR_MASK);
      uint8_t slot_num = *(const uint8_t *)packet_data;
      const char *name = (const char *)packet_data + 1;
      data_partition.write_begin(slot_num, packet_length - 1, name);
      write_token = packet_source;

      if (in_menu) {
        // Refresh the menu, because write_begin erases the old contents of the
        // slot.
        magic_io_signal_configuration_changed();
      }

      return encoder.finalize();
    }
    case CLI_PACKET_TYPE_EMULATOR_WRITE_DATA: {
      if (packet_length == 0) {
        return {nullptr, 0};  // Malformed request: do not reply.
      }

      encoder.begin(CLI_PACKET_TYPE_EMULATOR_WRITE_DATA ^
                    CLI_PACKET_TYPE_REPLY_XOR_MASK);
      if (write_token == packet_source) {
        const uint8_t *buf = (const uint8_t *)packet_data;
        for (uint i = 0; i < packet_length; i++) {
          data_partition.write_data(buf[i]);
        }
        encoder.push("OK", 2);
      } else {
        encoder.push("TOKEN", 5);
      }
      return encoder.finalize();
    }
    case CLI_PACKET_TYPE_EMULATOR_WRITE_END: {
      encoder.begin(CLI_PACKET_TYPE_EMULATOR_WRITE_END ^
                    CLI_PACKET_TYPE_REPLY_XOR_MASK);
      if (write_token == packet_source) {
        data_partition.write_end();

        if (in_menu) {
          // Refresh the menu, to reflect the new contents.
          magic_io_signal_configuration_changed();
        }

        encoder.push("OK", 2);
      } else {
        encoder.push("TOKEN", 5);
      }
      return encoder.finalize();
    }
    case CLI_PACKET_TYPE_EMULATOR_WIRELESS_CONFIG: {
      if (packet_length != 32 + 63) {
        return {nullptr, 0};  // Malformed request: do not reply.
      }

      encoder.begin(CLI_PACKET_TYPE_EMULATOR_WIRELESS_CONFIG ^
                    CLI_PACKET_TYPE_REPLY_XOR_MASK);
#if ROM_EMULATOR_WITH_WIRELESS == 1
      ConfigurationPartition::WirelessConfig new_config;
      memset(&new_config, 0x00, sizeof(new_config));
      memcpy(new_config.ssid, packet_data, 32);
      memcpy(new_config.psk, (const uint8_t *)packet_data + 32, 63);

      if (new_config.ssid[0] == '\0') {
        new_config.type = ConfigurationPartition::WirelessConfig::NotConfigured;
      } else if (new_config.psk[0] == '\0') {
        new_config.type = ConfigurationPartition::WirelessConfig::OpenNetwork;
      } else {
        new_config.type = ConfigurationPartition::WirelessConfig::WpaNetwork;
      }

      write_token = packet_source;
      data_partition.set_wireless_config(new_config);

      reload_wireless_config();
      encoder.push("OK", 2);
#else
      encoder.push("NOTW", 4);
#endif
      return encoder.finalize();
    }
    default: {  // Unknown packet_type.
      return {0, 0};
    }
  }
}

#if ROM_EMULATOR_WITH_WIRELESS == 1
class TcpClient {
 public:
  explicit TcpClient(tcp_pcb *pcb, CliProtocolDecoder *decoder)
      : pcb_(pcb), decoder_(decoder), closed_(false) {
    tcp_nagle_disable(pcb_);

    tcp_arg(pcb_, this);
    tcp_recv(pcb_,
             [](void *arg, struct tcp_pcb *tpcb, struct pbuf *p,
                err_t err) -> err_t {
               assert(err == ERR_OK);

               TcpClient *me = (TcpClient *)arg;

               bool keep_open = me->on_recv(p);
               if (p != nullptr) {
                 tcp_recved(me->pcb_, p->tot_len);
                 pbuf_free(p);
               }

               if (keep_open) {
                 return ERR_OK;
               } else {
                 return me->close();
               }
             });
  }

  ~TcpClient() { close(); }

 private:
  err_t close() {
    if (closed_) {
      return ERR_OK;
    } else {
      closed_ = true;
      tcp_abort(pcb_);
      return ERR_ABRT;
    }
  }

  bool on_recv(pbuf *p) {
    if (closed_ || p == nullptr) {
      return false;
    }

    uint8_t rx_buf[16];
    for (uint16_t pos = 0; pos < p->tot_len; pos += sizeof(rx_buf)) {
      uint16_t len = pbuf_copy_partial(p, rx_buf, sizeof(rx_buf), pos);
      for (uint16_t i = 0; i < len; i++) {
        switch (tcp_decoder.push(rx_buf[i])) {
          case CliProtocolDecoder::PushResult::Idle: {
            break;
          }
          case CliProtocolDecoder::PushResult::Error: {
            return false;
          }
          case CliProtocolDecoder::PushResult::PacketAvailable: {
            auto [reply_data, reply_length] = handle_packet(
                tcp_decoder.get_packet_type(), tcp_decoder.get_packet_data(),
                tcp_decoder.get_packet_length(), PacketSource::TcpClient);
            tcp_write(pcb_, reply_data, reply_length, TCP_WRITE_FLAG_COPY);
            tcp_output(pcb_);
            break;
          }
        }
      }
    }

    return true;
  }

  tcp_pcb *pcb_;
  CliProtocolDecoder *decoder_;
  bool closed_;
};

err_t on_tcp_client_accepted(void *arg, tcp_pcb *pcb, err_t err) {
  assert(pcb != nullptr);
  assert(err == ERR_OK);

  // There is only one CliProtocolDecoder instance (statically allocated)
  // dedicated to TCP clients. Let's be sure that there is never more than one
  // TCP client connected at the same time.
  static std::unique_ptr<TcpClient> curr_client;
  curr_client.reset();

  // Reset the decoder state.
  tcp_decoder.reset();

  curr_client = std::make_unique<TcpClient>(pcb, &tcp_decoder);
  return ERR_OK;
}
#endif

static void load_rom_from_data_partition() {
  const ConfigurationPartition::RomInfo &info =
      data_partition.get_rom_info(selected_boot_slot_num);
  const uint8_t *src = data_partition.get_rom_contents(selected_boot_slot_num);
  for (uint32_t i = 0; i < MAX_ROM_SIZE && i < info.size; i++) {
    romemu_write(i, src[i]);
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

#if ROM_EMULATOR_IS_INTERACTIVE == 1
  // Locate and open the data partition.
  bool partition_ok = data_partition.open();

  magic_io_prepare_rom(partition_ok ? MAGIC_IO_DESIRED_STATE_MAIN_MENU
                                    : MAGIC_IO_DESIRED_STATE_PARTITION_ERROR);
  in_menu = true;
  can_accept_boot_command = partition_ok;
#endif

  romemu_start();
  stdio_init_all();

#if ROM_EMULATOR_WITH_WIRELESS == 1
  if (partition_ok) {
    cyw43_wifi_pm(&cyw43_state, CYW43_PERFORMANCE_PM);
    reload_wireless_config();

    // Start listening for TCP connections.
    tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    assert(pcb != nullptr);
    tcp_bind(pcb, IP4_ADDR_ANY, CLI_PROTOCOL_TCP_PORT);
    pcb = tcp_listen(pcb);
    assert(pcb != nullptr);
    tcp_accept(pcb, on_tcp_client_accepted);
  }
#endif

  absolute_time_t next_toggle = get_absolute_time();
  bool led_on = true;
  while (1) {
    // Blink at 1 Hz (if in menu) or 2 Hz (otherwise).
    absolute_time_t now = get_absolute_time();
    if (absolute_time_diff_us(next_toggle, now) >= 0) {
      next_toggle = delayed_by_ms(next_toggle, in_menu ? 500 : 250);
      led_on = !led_on;
      led_set(led_on);
    }

    // Process magic I/O protocol if running the menu ROM.
    if (in_menu) {
      uint num_samples = trace_collect(TRACE_MAX_SAMPLES,
                                       make_timeout_time_us(150), trace_buf);
      MagicIoSignal signal = MagicIoSignal::None;
      if (num_samples == TRACE_MAX_SAMPLES) {
        signal = magic_io_analyze_traces(trace_buf, num_samples);
      }

      switch (signal) {
        case MagicIoSignal::None: {
          break;
        }
        case MagicIoSignal::UserRequestedBoot0... MagicIoSignal::
            UserRequestedBoot15: {
          selected_boot_slot_num =
              (uint)signal - (uint)MagicIoSignal::UserRequestedBoot0;
          bool slot_is_present =
              data_partition.get_rom_info(selected_boot_slot_num).is_present();
          if (can_accept_boot_command) {
            magic_io_set_desired_state(
                slot_is_present ? MAGIC_IO_DESIRED_STATE_BOOT_TRAMPOLINE
                                : MAGIC_IO_DESIRED_STATE_EMPTY_SLOT_ERROR);
            can_accept_boot_command = false;
          }
          break;
        }
        case MagicIoSignal::UserRequestedClientMode: {
          magic_io_set_desired_state(MAGIC_IO_DESIRED_STATE_CLIENT_MODE);
          break;
        }
        case MagicIoSignal::InTrampoline: {
          romemu_stop();
          in_menu = false;

          load_rom_from_data_partition();

          romemu_start();
          break;
        }
        // Interpret bytes received over magic I/O's serial tunnel with the
        // client protocol.
        case MagicIoSignal::SerialRx00... MagicIoSignal::SerialRxFF: {
          uint8_t rx_byte = (uint)signal - (uint)MagicIoSignal::SerialRx00;
          switch (magic_io_decoder.push(rx_byte)) {
            case CliProtocolDecoder::PushResult::Idle: {
              break;
            }
            case CliProtocolDecoder::PushResult::Error: {
              magic_io_decoder.reset();
              break;
            }
            case CliProtocolDecoder::PushResult::PacketAvailable: {
              auto [reply_data, reply_length] = handle_packet(
                  magic_io_decoder.get_packet_type(),
                  magic_io_decoder.get_packet_data(),
                  magic_io_decoder.get_packet_length(), PacketSource::MagicIo);
              for (uint i = 0; i < reply_length; i++) {
                magic_io_enqueue_serial_tx(reply_data[i]);
              }
              break;
            }
          }
          break;
        }
        case MagicIoSignal::ConfigurationDataRom0... MagicIoSignal::
            ConfigurationDataRom15: {
          uint slot_num =
              (uint)signal - (uint)MagicIoSignal::ConfigurationDataRom0;

          const ConfigurationPartition::RomInfo &src =
              data_partition.get_rom_info(slot_num);
          MAGIC_IO_CONFIGURATION_DATA_t buf = {};
          if (src.is_present()) {
            buf.rom.is_present = 1;
            buf.rom.name_length = src.name_length;
            memcpy(buf.rom.name, src.name,
                   std::min<size_t>(src.name_length, sizeof(buf.rom.name)));
          } else {
            buf.rom.is_present = 0;
          }

          magic_io_fill_configuration_block(buf);
          break;
        }
        case MagicIoSignal::ConfigurationDataNetwork: {
          MAGIC_IO_CONFIGURATION_DATA_t buf = {};
#if ROM_EMULATOR_WITH_WIRELESS == 1
          if (!data_partition.get_wireless_config().is_configured()) {
            buf.network.status = MAGIC_IO_WIRELESS_STATUS_NOT_CONFIGURED;
          } else if (!netif_is_link_up(&netif_list[0])) {
            buf.network.status = MAGIC_IO_WIRELESS_STATUS_NOT_CONNECTED;
          } else if (const ip4_addr_t *ip_addr = netif_ip4_addr(&netif_list[0]);
                     !ip4_addr_isany(ip_addr)) {
            buf.network.status = MAGIC_IO_WIRELESS_STATUS_CONNECTED;
            memcpy(&buf.network.ip, ip_addr, 4);
          } else {
            buf.network.status = MAGIC_IO_WIRELESS_STATUS_WAITING_FOR_IP;
          }
#else
          buf.network.status = MAGIC_IO_WIRELESS_STATUS_NOT_PRESENT;
#endif
          magic_io_fill_configuration_block(buf);
        }
      }
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
              stdio_decoder.get_packet_length(), PacketSource::Stdio);
          for (uint i = 0; i < reply_length; i++) {
            stdio_putchar_raw(reply_data[i]);
          }
          break;
        }
      }
    }

#if ROM_EMULATOR_WITH_WIRELESS == 1
    cyw43_arch_poll();
#endif
  }
}
