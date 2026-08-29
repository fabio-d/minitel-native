#include "nvram.h"

#include <assert.h>
#include <hardware/clocks.h>
#include <hardware/pio.h>
#include <limits.h>
#include <pico/binary_info.h>
#include <pico/time.h>

#include "nvram.pio.h"
#include "pin-map.h"

static const PIO pio = pio2;
static constexpr uint sm = 1;
static constexpr double spi_clock = 15'000'000;

bi_decl(bi_program_feature("Non-volatile RAM"));
bi_decl(bi_1pin_with_name(PIN_NVRAM_SCK, "NVRAM_SCK"));
bi_decl(bi_1pin_with_name(PIN_NVRAM_MOSI, "NVRAM_MOSI"));
bi_decl(bi_1pin_with_name(PIN_NVRAM_MISO, "NVRAM_MISO"));
bi_decl(bi_1pin_with_name(PIN_NVRAM_CS, "~NVRAM_CS"));

// Check nvram_dual32's requirement is satisfied.
static_assert(PIN_NVRAM_MISO == PIN_NVRAM_MOSI + 1,
              "NVRAM_MOSI and NVRAM_MISO must be consecutive");

// What program is currently loaded and the address at which it has been loaded.
static const pio_program_t* prog = nullptr;
static uint prog_load_offset;

// Loads and starts a PIO program, replacing the previous one.
static void start_program(
    const pio_program_t* new_prog,
    pio_sm_config (*new_prog_get_default_config)(uint offset),
    uint new_prog_offset_entry_point) {
  // If the requested program is already running, there is nothing to do.
  if (new_prog == prog) {
    return;
  }

  // Stop and unload the old program, if there is one.
  if (prog != nullptr) {
    pio_sm_set_enabled(pio, sm, false);
    pio_remove_program(pio, prog, prog_load_offset);
  }

  // Load the new program.
  prog_load_offset = pio_add_program(pio, new_prog);
  prog = new_prog;
  // Configure the parameters.
  pio_sm_config cfg = new_prog_get_default_config(prog_load_offset);

  // Set SPI clock frequency.
  double ratio = clock_get_hz(clk_sys) / spi_clock;
  sm_config_set_clkdiv(&cfg, ratio / nvram_clocks_per_sck);

  // Assign pin numbers.
  sm_config_set_set_pin_base(&cfg, PIN_NVRAM_CS);
  sm_config_set_sideset_pin_base(&cfg, PIN_NVRAM_SCK);
  sm_config_set_out_pin_base(&cfg, PIN_NVRAM_MOSI);
  pio_sm_set_pins(pio, sm, 1 << PIN_NVRAM_CS);  // initially CS=1
  pio_sm_set_consecutive_pindirs(pio, sm, PIN_NVRAM_CS, 1, true);
  pio_sm_set_consecutive_pindirs(pio, sm, PIN_NVRAM_SCK, 1, true);
  if (prog == &nvram_dual32_program) {
    // Special case: configure both MOSI *and* MISO as output pins.
    pio_sm_set_consecutive_pindirs(pio, sm, PIN_NVRAM_MOSI, 2, true);
  } else {
    pio_sm_set_consecutive_pindirs(pio, sm, PIN_NVRAM_MOSI, 1, true);

    sm_config_set_in_pin_base(&cfg, PIN_NVRAM_MISO);
    pio_sm_set_consecutive_pindirs(pio, sm, PIN_NVRAM_MISO, 1, false);
  }

  // Initialize and start the state machine.
  uint entry_point = prog_load_offset + new_prog_offset_entry_point;
  pio_sm_init(pio, sm, entry_point, &cfg);
  pio_sm_set_enabled(pio, sm, true);
}

// Prepares the PIO state machine for a transaction of one or more bytes, by
// ensuring that the nvram_xfer8 program is running and awaiting for the given
// number of bytes.
//
// The nvram_xfer8 PIO program automatically drives CS low before sending the
// first byte and then drive CS back to high after the given amount of bytes has
// been transferred.
static void xfer8_start_transaction(uint num_bytes) {
  start_program(&nvram_xfer8_program, nvram_xfer8_program_get_default_config,
                nvram_xfer8_offset_entry_point);

  uint total_bits = num_bytes * 8;
  pio_sm_put(pio, sm, total_bits - 1);
}

// Simultaneously sends and receives one byte in a xfer8 transaction.
static uint8_t xfer8_continue(uint8_t output_data) {
  pio_sm_put(pio, sm, (uint32_t)output_data << 24);
  uint32_t input_data = pio_sm_get_blocking(pio, sm);
  return (uint8_t)input_data;
}

// Brings the 23LCV512 back into normal SPI mode by executing a RSTIO command.
//
// The PIO program, in combination with the pull-up method, makes it safe to
// execute this function even if the 23LCV512 is already in normal SPI mode (in
// which case, it has no effect).
static void rstio_run() {
  // Activate pull-up on MISO.
  gpio_set_pulls(PIN_NVRAM_MISO, true, false);

  start_program(&nvram_rstio_program, nvram_rstio_program_get_default_config,
                nvram_rstio_offset_entry_point);
  pio_sm_get_blocking(pio, sm);  // wait for completion

  // Deactivate pull-up on MISO.
  gpio_set_pulls(PIN_NVRAM_MISO, false, false);
}

void nvram_setup() {
  // Claim the state machine.
  pio_sm_claim(pio, sm);

  // Bypass input synchronizer (SPI manages clocking by itself).
  hw_set_bits(&pio->input_sync_bypass, 1u << PIN_NVRAM_MISO);

  // Claim output GPIOs.
  pio_gpio_init(pio, PIN_NVRAM_CS);
  pio_gpio_init(pio, PIN_NVRAM_SCK);
  pio_gpio_init(pio, PIN_NVRAM_MOSI);

  // Set MISO as input.
  gpio_init(PIN_NVRAM_MISO);
  gpio_set_dir(PIN_NVRAM_MISO, GPIO_IN);

  // Issue a RSTIO command to exit Dual Serial Mode to bring the device into
  // normal SPI mode. This command has no effect on the 23LCV512 if it is
  // already in normal SPI mode.
  rstio_run();

  // Issue a WRMR command to set "Sequential Operation" mode.
  xfer8_start_transaction(2);
  xfer8_continue(0x01);
  xfer8_continue(0b01 << 6);
}

void nvram_read_burst(uint16_t addr, size_t n_bytes,
                      void (*cb)(uint16_t addr, uint8_t data)) {
  xfer8_start_transaction(3 + n_bytes);

  // Issue a READ command.
  xfer8_continue(0x03);
  xfer8_continue((uint8_t)(addr >> 8));
  xfer8_continue((uint8_t)addr);
  while (n_bytes-- != 0) {
    cb(addr++, xfer8_continue(0));
  }
}

void nvram_enter_write_fast_mode() {
  // Issue a WRMR command to set "Byte Operation" mode, so that power cuts that
  // result in SCK continuing to oscillate while a write is in progress do not
  // end up overwriting adjacent data too.
  xfer8_start_transaction(2);
  xfer8_continue(0x01);
  xfer8_continue(0b00 << 6);

  // Issue an EDIO command to enter Dual Serial Mode.
  xfer8_start_transaction(1);
  xfer8_continue(0x3B);

  // Start PIO program that performs fixed-size (32-bit) output-only
  // transactions in dual serial mode.
  start_program(&nvram_dual32_program, nvram_dual32_program_get_default_config,
                nvram_dual32_offset_entry_point);

  // MISO is now an output.
  gpio_set_dir(PIN_NVRAM_MISO, GPIO_OUT);
  pio_gpio_init(pio, PIN_NVRAM_MISO);
}

void nvram_write_fast(uint16_t addr, uint8_t data) {
  // Issue a WRITE command (it is assumed that the nvram_dual32 program has
  // already been started with nvram_enter_write_fast_mode).
  pio_sm_put(pio, sm, 0x02000000 | ((uint32_t)addr << 8) | data);
}
