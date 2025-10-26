#include "romemu.h"

#include <hardware/dma.h>
#include <hardware/pio.h>
#include <pico/binary_info.h>
#include <pico/stdlib.h>

#include <atomic>

#include "pin-map.h"
#include "romemu.pio.h"

static constexpr uint32_t PIN_ADDR_AD_MASK =
    (1 << PIN_AD0) | (1 << PIN_AD1) | (1 << PIN_AD2) | (1 << PIN_AD3) |
    (1 << PIN_AD4) | (1 << PIN_AD5) | (1 << PIN_AD6) | (1 << PIN_AD7);
static_assert(PIN_ADDR_AD_MASK >> PIN_AD_BASE == 0xff,
              "Data lines must be consecutive");

static constexpr uint32_t PIN_ADDR_A_MASK =
    (1 << PIN_A8) | (1 << PIN_A9) | (1 << PIN_A10) | (1 << PIN_A11) |
    (1 << PIN_A12) | (1 << PIN_A13) | (1 << PIN_A14) | (1 << PIN_A15);
static constexpr uint32_t PIN_ADDR_ALL_MASK =
    PIN_ADDR_AD_MASK | PIN_ADDR_A_MASK;
static_assert(PIN_ADDR_ALL_MASK == 0xffff,
              "Address lines must start from GPIO0 and be consecutive");

bi_decl(bi_pin_mask_with_names(PIN_ADDR_ALL_MASK, PIN_ADDR_ALL_NAMES));
bi_decl(bi_1pin_with_name(PIN_NOPEN, "~NOPEN"));
bi_decl(bi_1pin_with_name(PIN_BUSEN, "~BUSEN"));
bi_decl(bi_1pin_with_name(PIN_ALE, "ALE"));
bi_decl(bi_1pin_with_name(PIN_PSEN, "~PSEN"));

// PIO resources.
static const PIO pio_serve = pio0;
static constexpr uint sm_out = 0;
static constexpr uint sm_dira = 1;
static constexpr uint sm_dirb = 2;
static const PIO pio_sense = pio1;
static constexpr uint sm_latch = 0;

// DMA resources.
static constexpr uint dma_addr = 0;
static constexpr uint dma_data = 1;

// ROM, stored as (pin-mapped address) -> (pin-mapped value).
static constexpr uint ROM_SIZE = 64 * 1024;
static std::atomic<uint8_t> rom[ROM_SIZE] [[gnu::aligned(ROM_SIZE)]];

// PC values to jump to activate/pause the sm_latch state machine.
static uint pc_latch_paused, pc_latch_active;

void romemu_setup() {
  // Initially fill the emulated ROM contents with 0xFF. Note that, in fact, we
  // will keep serving NOPs (0x00) until romemu_start is called.
  for (uint i = 0; i < ROM_SIZE; i++) {
    romemu_write(i, 0xFF);
  }

  // Claim the resources that we will need.
  pio_sm_claim(pio_serve, sm_out);
  pio_sm_claim(pio_serve, sm_dira);
  pio_sm_claim(pio_serve, sm_dirb);
  pio_sm_claim(pio_sense, sm_latch);
  dma_channel_claim(dma_addr);
  dma_channel_claim(dma_data);

  // Load the programs into the PIO engine.
  uint prog_out = pio_add_program(pio_serve, &romemu_out_program);
  uint prog_dir = pio_add_program(pio_serve, &romemu_dir_program);
  uint prog_latch = pio_add_program(pio_sense, &romemu_latch_program);
  pio_sm_config cfg_out = romemu_out_program_get_default_config(prog_out);
  pio_sm_config cfg_dira = romemu_dir_program_get_default_config(prog_dir);
  pio_sm_config cfg_dirb = romemu_dir_program_get_default_config(prog_dir);
  pio_sm_config cfg_latch = romemu_latch_program_get_default_config(prog_latch);

  // Remember the addresses of these two labels.
  pc_latch_paused = prog_latch + romemu_latch_offset_paused;
  pc_latch_active = prog_latch + romemu_latch_offset_active;

  // Assign pin numbers.
  sm_config_set_out_pins(&cfg_out, PIN_AD_BASE, 8);
  sm_config_set_set_pins(&cfg_out, PIN_AD_BASE, 8);
  sm_config_set_jmp_pin(&cfg_dira, PIN_PSEN);
  sm_config_set_jmp_pin(&cfg_dirb, PIN_PSEN);
  sm_config_set_sideset_pins(&cfg_dira, PIN_AD_BASE);
  sm_config_set_sideset_pins(&cfg_dirb, PIN_AD_BASE + 4);
  sm_config_set_jmp_pin(&cfg_latch, PIN_ALE);
  pio_sm_set_consecutive_pindirs(pio_serve, sm_dira, PIN_AD_BASE, 4, false);
  pio_sm_set_consecutive_pindirs(pio_serve, sm_dirb, PIN_AD_BASE + 4, 4, false);

  // Set the initial output value to zero, for two reasons:
  // - an all-zero value is interpreted by the Minitel CPU as a (harmless) NOP,
  //   which safely "parks" it until we start serving the real ROM.
  // - to avoid bus conflicts while taking over from the SN74HCT541, as it emits
  //   zeros too.
  pio_sm_set_pins(pio_serve, sm_out, 0x00);
  pio_serve->rxf_putget[sm_out][0] = 0x00;

  // Claim tristate GPIOs.
  for (int i = 0; i < 8; i++) {
    pio_gpio_init(pio_serve, PIN_AD_BASE + i);
  }

  // Setup chained DMA: dma_addr will read the address latched by sm_latch and
  // then immediately trigger dma_data, which reads from it and then pushes the
  // value to sm_out.
  dma_channel_config_t cfg_addr = dma_channel_get_default_config(dma_addr);
  dma_channel_config_t cfg_data = dma_channel_get_default_config(dma_data);
  channel_config_set_transfer_data_size(&cfg_addr, DMA_SIZE_32);
  channel_config_set_read_increment(&cfg_addr, false);
  channel_config_set_write_increment(&cfg_addr, false);
  channel_config_set_dreq(&cfg_addr, pio_get_dreq(pio_sense, sm_latch, false));
  channel_config_set_high_priority(&cfg_addr, true);
  channel_config_set_transfer_data_size(&cfg_data, DMA_SIZE_8);
  channel_config_set_read_increment(&cfg_data, false);
  channel_config_set_write_increment(&cfg_data, false);
  channel_config_set_dreq(&cfg_data, pio_get_dreq(pio_serve, sm_out, true));
  channel_config_set_chain_to(&cfg_data, dma_addr);
  channel_config_set_high_priority(&cfg_data, true);
  dma_channel_configure(
      dma_addr, &cfg_addr, &dma_hw->ch[dma_data].al3_read_addr_trig,
      &pio_sense->rxf[sm_latch], dma_encode_transfer_count(1), false);
  dma_channel_configure(dma_data, &cfg_data, &pio_serve->rxf_putget[sm_out][0],
                        nullptr /* set by dma_addr */,
                        dma_encode_transfer_count(1), false);

  // Take control of the NOPEN output pin (which is externally pulled-down).
  // Let's start with maintaining 0 as an output, so that the SN74HCT541 doesn't
  // stop generating NOP (i.e. 0x00) yet. We have to be careful to never emit
  // conflicting non-0x00 values on the bus while the SN74HCT541 is active.
  // We will disable the SN74HCT541 later in this function, when we have
  // completed our initialization.
  gpio_init(PIN_NOPEN);
  gpio_put(PIN_NOPEN, 0);
  gpio_set_dir(PIN_NOPEN, GPIO_OUT);

  // Tell the two SN74CB3T3384 chips to stop isolating us from the bus.
  gpio_init(PIN_BUSEN);
  gpio_put(PIN_BUSEN, 0);
  gpio_set_dir(PIN_BUSEN, GPIO_OUT);

  // Set the other GPIOs as inputs.
  gpio_init(PIN_ALE);
  gpio_set_dir(PIN_ALE, GPIO_IN);
  gpio_init(PIN_PSEN);
  gpio_set_dir(PIN_PSEN, GPIO_IN);
  gpio_init_mask(PIN_ADDR_A_MASK);
  gpio_set_dir_in_masked(PIN_ADDR_A_MASK);

  // Start the state machines. Start sm_out first and then wait a bit to be sure
  // that the initial output value of 0x00 has propagated.
  uint out_entry_point = prog_out + romemu_out_offset_entry_point;
  uint dir_entry_point = prog_dir + romemu_dir_offset_entry_point;
  uint latch_entry_point = prog_latch + romemu_latch_offset_entry_point;
  pio_sm_init(pio_serve, sm_out, out_entry_point, &cfg_out);
  pio_sm_init(pio_serve, sm_dira, dir_entry_point, &cfg_dira);
  pio_sm_init(pio_serve, sm_dirb, dir_entry_point, &cfg_dirb);
  pio_sm_init(pio_sense, sm_latch, latch_entry_point, &cfg_latch);
  pio_enable_sm_mask_in_sync(pio_serve, 1 << sm_out);
  pio_enable_sm_mask_in_sync(pio_serve, (1 << sm_dira) | (1 << sm_dirb));
  pio_enable_sm_mask_in_sync(pio_sense, 1 << sm_latch);

  // Set prefix in sm_latch and wait until it starts spinning in the "paused"
  // loop.
  pio_sm_put(pio_sense, sm_latch, (uintptr_t)rom >> 16);
  while (pio_sense->sm[sm_latch].addr != pc_latch_paused) {
    tight_loop_contents();
  }

  // With the state machines now running, we are now emitting NOPs (0x00) too.
  // We can tell the SN74HCT541 to stop emitting its own NOPs.
  sleep_us(100);
  gpio_put(PIN_NOPEN, 1);

  // Give SN74HCT541 extra time to fully deactivate. After this, we can emit
  // non-0x00 values without conflicting with it.
  sleep_us(100);
}

void romemu_start() {
  // Start the DMA engine too.
  dma_channel_start(dma_addr);

  // Start the state machine that emits latched addresses.
  pio_sm_exec(pio_sense, sm_latch, pio_encode_jmp(pc_latch_active));
}

void romemu_stop() {
  // Save the current values of the CTRL register of both DMA channels.
  uint32_t old_ctrl_addr = dma_channel_hw_addr(dma_addr)->al1_ctrl;
  uint32_t old_ctrl_data = dma_channel_hw_addr(dma_data)->al1_ctrl;

  // Stop triggering.
  pio_sm_exec(pio_sense, sm_latch, pio_encode_jmp(pc_latch_paused));
  while (pio_sense->sm[sm_latch].addr != pc_latch_paused) {
    tight_loop_contents();
  }

  // Stop the DMA engine (with workaround for errata RP2350-E5).
  dma_channel_hw_addr(dma_addr)->al1_ctrl = old_ctrl_addr & ~1;  // clear EN bit
  dma_channel_hw_addr(dma_data)->al1_ctrl = old_ctrl_data & ~1;  // clear EN bit
  dma_hw->abort = (1 << dma_addr) | (1 << dma_data);
  while (dma_hw->abort != 0) {
    tight_loop_contents();
  }

  // Start emitting 0x00 (NOPs) again.
  pio_serve->rxf_putget[sm_out][0] = 0x00;

  // Undo the workaround for errata RP2350-E5 and make the channels ready to
  // be re-triggered.
  dma_channel_hw_addr(dma_addr)->al1_ctrl = old_ctrl_addr;
  dma_channel_hw_addr(dma_data)->al1_ctrl = old_ctrl_data;
}

void romemu_write(uint16_t address, uint8_t value) {
  // Transform the logical address and value into the corresponding pin-mapped
  // permutation.
  uint16_t address_pin_values = pin_map_address(address);
  uint8_t value_pin_values = pin_map_data(value);

  // Atomically update the rom array.
  rom[address_pin_values].store(value_pin_values);
}
