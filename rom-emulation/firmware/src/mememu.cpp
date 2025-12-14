#include "mememu.h"

#include <hardware/dma.h>
#include <hardware/pio.h>
#include <pico/binary_info.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>

#include <atomic>

#include "mememu-common.pio.h"
#include "pin-map.h"

#if ROM_EMULATOR_PROVIDES_RAM == 1
#include "mememu-with-ram.pio.h"
#else
#include "mememu-without-ram.pio.h"
#endif

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
bi_decl(bi_1pin_with_name(PIN_ALE, "ALE"));
bi_decl(bi_1pin_with_name(PIN_PSEN, "~PSEN"));

#if ROM_EMULATOR_HAS_BUS_SWITCH == 1
bi_decl(bi_program_feature("Bus Switch control"));
bi_decl(bi_1pin_with_name(PIN_NOPEN, "~NOPEN"));
bi_decl(bi_1pin_with_name(PIN_BUSEN, "~BUSEN"));
#endif

#if ROM_EMULATOR_PROVIDES_RAM == 1
// We emulate an active-high RAM, selected by A15.
static constexpr uint PIN_RAM_EN = PIN_A15;
bi_decl(bi_program_feature("Emulated RAM, selected by A15=HIGH"));
bi_decl(bi_1pin_with_name(PIN_WR, "~WR"));
bi_decl(bi_1pin_with_name(PIN_RD, "~RD"));

// The mememu_dir PIO program requires PSEN, WR and RD to be consecutive,
// because it uses them as a 3-bit index in a jump table.
static_assert(PIN_WR == PIN_PSEN + 1 && PIN_RD == PIN_PSEN + 2,
              "PSEN, WR and RD must be consecutive");
#endif

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

// ROM and RAM contents, stored as consecutive pairs:
// - (2 * pin-mapped address + 0) -> (pin-mapped RAM value)
// - (2 * pin-mapped address + 1) -> (pin-mapped ROM value)
static constexpr uint MEMARRAY_SHIFT = 17;
static constexpr uint MEMARRAY_SIZE = 2 * MAX_MEM_SIZE;  // ROM + RAM
static std::atomic<uint8_t> mem[MEMARRAY_SIZE] [[gnu::aligned(MEMARRAY_SIZE)]];
static_assert(MEMARRAY_SIZE == 1 << MEMARRAY_SHIFT);

// PC values to jump to activate/pause the sm_latch state machine.
static uint pc_latch_paused, pc_latch_active;

[[gnu::noinline, gnu::noreturn]]
static void __scratch_x("core1_worker_task") core1_worker_task() {
  while (true) {
#if ROM_EMULATOR_PROVIDES_RAM == 1
    // Wait for WR to go low.
    uint32_t value;
    do {
      value = gpio_get_all();
    } while ((value & (1 << PIN_WR)) != 0);

    // Get the latched address.
    auto storage = (std::atomic<uint8_t>*)dma_hw->ch[dma_data].al1_read_addr;

    // Write the new RAM value into the mem array.
    *storage = value >> PIN_AD_BASE;

    // Wait for WR to go high.
    do {
      value = gpio_get_all();
    } while ((value & (1 << PIN_WR)) == 0);
#else
    __wfe();
#endif
  }
}

void mememu_setup() {
  // Initially fill the emulated ROM and RAM contents with 0xFF. Note that, in
  // fact, we will keep serving 0x00 until mememu_start is called.
  for (uint i = 0; i < MAX_MEM_SIZE; i++) {
    mememu_write_rom(i, 0xFF);
    mememu_write_ram(i, 0xFF);
  }

  // Claim the resources that we will need.
  pio_sm_claim(pio_serve, sm_out);
  pio_sm_claim(pio_serve, sm_dira);
  pio_sm_claim(pio_serve, sm_dirb);
  pio_sm_claim(pio_sense, sm_latch);
  dma_channel_claim(dma_addr);
  dma_channel_claim(dma_data);

  // Load the programs into the PIO engine.
  uint prog_out = pio_add_program(pio_serve, &mememu_out_program);
  uint prog_dir = pio_add_program(pio_serve, &mememu_dir_program);
  uint prog_latch = pio_add_program(pio_sense, &mememu_latch_program);
  pio_sm_config cfg_out = mememu_out_program_get_default_config(prog_out);
  pio_sm_config cfg_dira = mememu_dir_program_get_default_config(prog_dir);
  pio_sm_config cfg_dirb = mememu_dir_program_get_default_config(prog_dir);
  pio_sm_config cfg_latch = mememu_latch_program_get_default_config(prog_latch);

  // Remember the addresses of these two labels.
  pc_latch_paused = prog_latch + mememu_latch_offset_paused;
  pc_latch_active = prog_latch + mememu_latch_offset_active;

  // Assign pin numbers.
  sm_config_set_out_pin_base(&cfg_out, PIN_AD_BASE);
  sm_config_set_in_pin_base(&cfg_dira, PIN_PSEN);
  sm_config_set_in_pin_base(&cfg_dirb, PIN_PSEN);
  sm_config_set_sideset_pins(&cfg_dira, PIN_AD_BASE);
  sm_config_set_sideset_pins(&cfg_dirb, PIN_AD_BASE + 4);
  sm_config_set_jmp_pin(&cfg_latch, PIN_ALE);
  pio_sm_set_consecutive_pindirs(pio_serve, sm_dira, PIN_AD_BASE, 4, false);
  pio_sm_set_consecutive_pindirs(pio_serve, sm_dirb, PIN_AD_BASE + 4, 4, false);
#if ROM_EMULATOR_PROVIDES_RAM == 1
  sm_config_set_jmp_pin(&cfg_out, PIN_RD);
  sm_config_set_jmp_pin(&cfg_dira, PIN_RAM_EN);
  sm_config_set_jmp_pin(&cfg_dirb, PIN_RAM_EN);
#endif

  // Set the initial output value to zero, for two reasons:
  // - an all-zero value is interpreted by the Minitel CPU as a (harmless) NOP,
  //   which safely "parks" it until we start serving the real ROM.
  // - to avoid bus conflicts while taking over from the SN74HCT541, as it emits
  //   zeros too.
  pio_sm_set_pins(pio_serve, sm_out, pin_map_data(0x00) << PIN_AD_BASE);

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
  channel_config_set_transfer_data_size(&cfg_data, DMA_SIZE_16);
  channel_config_set_bswap(&cfg_data, true);
  channel_config_set_read_increment(&cfg_data, false);
  channel_config_set_write_increment(&cfg_data, false);
  channel_config_set_dreq(&cfg_data, pio_get_dreq(pio_serve, sm_out, true));
  channel_config_set_chain_to(&cfg_data, dma_addr);
  channel_config_set_high_priority(&cfg_data, true);
  dma_channel_configure(
      dma_addr, &cfg_addr, &dma_hw->ch[dma_data].al3_read_addr_trig,
      &pio_sense->rxf[sm_latch], dma_encode_transfer_count(1), false);
  dma_channel_configure(dma_data, &cfg_data, &pio_serve->rxf_putget[sm_out][0],
                        &mem /* set at runtime by dma_addr */,
                        dma_encode_transfer_count(1), false);

#if ROM_EMULATOR_HAS_BUS_SWITCH == 1
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
#endif

  // Set the other GPIOs as inputs.
  gpio_init(PIN_ALE);
  gpio_set_dir(PIN_ALE, GPIO_IN);
  gpio_init(PIN_PSEN);
  gpio_set_dir(PIN_PSEN, GPIO_IN);
#if ROM_EMULATOR_PROVIDES_RAM == 1
  gpio_init(PIN_WR);
  gpio_set_dir(PIN_WR, GPIO_IN);
  gpio_init(PIN_RD);
  gpio_set_dir(PIN_RD, GPIO_IN);
#endif
  gpio_init_mask(PIN_ADDR_A_MASK);
  gpio_set_dir_in_masked(PIN_ADDR_A_MASK);

  // Initialize and start the state machines.
  uint out_entry_point = prog_out + mememu_out_offset_entry_point;
  uint dir_entry_point = prog_dir + mememu_dir_offset_entry_point;
  uint latch_entry_point = prog_latch + mememu_latch_offset_entry_point;
  pio_sm_init(pio_serve, sm_out, out_entry_point, &cfg_out);
  pio_sm_init(pio_serve, sm_dira, dir_entry_point, &cfg_dira);
  pio_sm_init(pio_serve, sm_dirb, dir_entry_point, &cfg_dirb);
  pio_sm_init(pio_sense, sm_latch, latch_entry_point, &cfg_latch);
  pio_serve->rxf_putget[sm_out][0] = 0x0000;  // must be done after pio_sm_init!
  pio_enable_sm_mask_in_sync(pio_serve, 1 << sm_out);
  pio_enable_sm_mask_in_sync(pio_serve, (1 << sm_dira) | (1 << sm_dirb));
  pio_enable_sm_mask_in_sync(pio_sense, 1 << sm_latch);

  // Set prefix in sm_latch and wait until it starts spinning in the "paused"
  // loop.
  pio_sm_put(pio_sense, sm_latch, (uintptr_t)mem >> MEMARRAY_SHIFT);
  while (pio_sense->sm[sm_latch].addr != pc_latch_paused) {
    tight_loop_contents();
  }

  // Start the worker function on core 1, dedicated to processing writes to the
  // emulated RAM.
  multicore_launch_core1(core1_worker_task);

#if ROM_EMULATOR_HAS_BUS_SWITCH == 1
  // With the state machines now running, we are now emitting NOPs (0x00) too.
  // We can tell the SN74HCT541 to stop emitting its own NOPs.
  sleep_us(100);
  gpio_put(PIN_NOPEN, 1);

  // Give SN74HCT541 extra time to fully deactivate. After this, we can emit
  // non-0x00 values without conflicting with it.
  sleep_us(100);
#endif
}

void mememu_start() {
  // Start the DMA engine too.
  dma_channel_start(dma_addr);

  // Start the state machine that emits latched addresses.
  pio_sm_exec(pio_sense, sm_latch, pio_encode_jmp(pc_latch_active));
}

void mememu_stop() {
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

  // Start emitting 0x00 again.
  pio_serve->rxf_putget[sm_out][0] = 0x0000;

  // Undo the workaround for errata RP2350-E5 and make the channels ready to
  // be re-triggered.
  dma_channel_hw_addr(dma_addr)->al1_ctrl = old_ctrl_addr;
  dma_channel_hw_addr(dma_data)->al1_ctrl = old_ctrl_data;
}

void mememu_write_rom(uint16_t address, uint8_t value) {
  // Transform the logical address and value into the corresponding pin-mapped
  // permutation.
  uint16_t address_pin_values = pin_map_address(address);
  uint8_t value_pin_values = pin_map_data(value);

  // Atomically update the mem array.
  mem[2 * address_pin_values + 1].store(value_pin_values);
}

void mememu_write_ram(uint16_t address, uint8_t value) {
  // Transform the logical address and value into the corresponding pin-mapped
  // permutation.
  uint16_t address_pin_values = pin_map_address(address);
  uint8_t value_pin_values = pin_map_data(value);

  // Atomically update the mem array.
  mem[2 * address_pin_values + 0].store(value_pin_values);
}
