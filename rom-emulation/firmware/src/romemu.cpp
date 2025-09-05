#include "romemu.h"

#include <hardware/pio.h>
#include <pico/binary_info.h>
#include <pico/multicore.h>

#include <atomic>

#include "pin-map.h"
#include "romemu.pio.h"

static constexpr uint32_t PIN_ADDR_AD_MASK =
    (1 << PIN_AD0) | (1 << PIN_AD1) | (1 << PIN_AD2) | (1 << PIN_AD3) |
    (1 << PIN_AD4) | (1 << PIN_AD5) | (1 << PIN_AD6) | (1 << PIN_AD7);
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
static const PIO pio = pio0;
static constexpr uint sm_a = 0;
static constexpr uint sm_b = 1;

// ROM, stored as (pin-mapped address) -> (PIO instructions for sm_a and sm_b)
// in a dedicated RAM region exclusively accessed by core1 while romemu is
// running.
//
// The value is a pair of PIO instructions for sm_a and sm_b that inject the
// corresponding value into the respective state machines.
static constexpr uint ROM_SIZE = 64 * 1024;
static std::atomic<uint32_t> rom[ROM_SIZE] [[gnu::section(
    ".ram2_uninitialized_data")]];

/*
 * This function is the core of the ROM emulation: it continuously senses the
 * input values on the GPIOs and selects what value to emit.
 *
 * Its execution is divided in two phases:
 * - Initial phase: we always reply with 0x00 (NOP). This phase ends as soon as
 *   address 0x0000 is latched. During this phase, the Minitel CPU will
 *   increment its program counter by 1 for each executed NOP (until 0xFFFF,
 *   after which it will wrap around to 0x0000).
 * - Operating phase: during this phase, we reply with the real ROM values.
 *
 * The initial "NOP slide" phase ensures that, no matter what program counter
 * the Minitel CPU happens to be at when we start, we let it safely reach the
 * ROM's entry point (address 0) without executing uncontrolled instructions.
 * This is necessary because we have no control over the Minitel CPU's reset
 * signal: the Minitel CPU might start to run before we are ready to serve the
 * ROM.
 *
 * Register usage
 * - Non-constant registers:
 *   - r0: Scratch register for short-lived values.
 *   - r1: Most recent GPIO readout.
 *   - r2: Most recent GPIO readout whose ALE bit was high. The lowest 16 bits
 *         of this register contain the latched address.
 *   - r3/r4: Encoded instructions for sm_a/sm_b.
 * - Constant values:
 *   - kAleToSign: Number to bits to left-shift a GPIO reading so that the ALE
 *                 bit (i.e. 1 << PIN_ALE) ends up at the position of the sign
 *                 bit (i.e. 1 << 31).
 * - Constant registers:
 *   - r6: Address of the "rom" array.
 *   - r8/r9: Address of PIO's SMx_INSTR registers for sm_a/sm_b.
 *
 * This function's code is stored in a RAM region that is exclusively accessed
 * by core1 (__scratch_x).
 */
[[gnu::noinline, gnu::noreturn]]
static void __scratch_x("romemu_worker_in_ram") core1_worker_task() {
  constexpr uint kAleToSign = 31 - PIN_ALE;
  register const std::atomic<uint32_t>* rom_in asm("r6") = rom;
  register io_rw_32* instr_a_in asm("r8") = &pio->sm[sm_a].instr;
  register io_rw_32* instr_b_in asm("r9") = &pio->sm[sm_b].instr;

  asm volatile(
      // Load the PIO instructions to serve address 0, but don't execute them
      // yet.
      "ldr r0, [r6, #0]\n"
      "uxth r3, r0\n"
      "lsrs r4, r0, #16\n"

      // Initialize the latch with non-zero address bits.
      "mov r2, #-1\n"

      // Initial synchronization: latch the GPIOs if ALE is high and wait for
      // ALE to become low *with address 0 latched*.
      "b 2f\n"
      "1:"                            // We only branch here if ALE is high.
      "mov r2, r1\n"                  // Latch the current GPIO readout.
      "2:\n"                          // This is the entry point of the loop.
      "mrc p0, #0, r1, c0, c8\n"      // r1 = gpio_get_all()
      "lsls r0, r1, %[kAleToSign]\n"  // Shift ALE into the sign position.
      "bmi 1b\n"                      // Branch if ALE is high.
      "uxth r0, r2\n"                 // Isolate the address bits.
      "cmp r0, #0\n"                  // Is the latched address zero?
      "bne 2b\n"                      // Keep waiting if not.

      // If we are here, it means that address 0 has been latched and ALE is
      // low. Serve the value at address 0 by executing the instructions we
      // had prepared.
      "str r3, [r8]\n"  // Write into sm_a.
      "str r4, [r9]\n"  // Write into sm_b.

      // Synchronization for subsequent instructions: latch the GPIOs if ALE is
      // high and wait for ALE to become low.
      "b 4f\n"
      "3:"                            // We only branch here if ALE is high.
      "mov r2, r1\n"                  // Latch the current GPIO readout.
      "4:\n"                          // This is the entry point of the loop.
      "mrc p0, #0, r1, c0, c8\n"      // r1 = gpio_get_all()
      "lsls r0, r1, %[kAleToSign]\n"  // Shift ALE into the sign position.
      "bmi 3b\n"                      // Branch if ALE is high.

      // Load the PIO instructions to serve the latched address.
      "uxth r0, r2\n"  // Isolate the address bits.
      "ldr r0, [r6, r0, lsl #2]\n"
      "uxth r3, r0\n"
      "lsrs r4, r0, #16\n"

      // Execute them.
      "str r3, [r8]\n"  // Write into sm_a.
      "str r4, [r9]\n"  // Write into sm_b.

      // Repeat main loop.
      "b 4b\n"
      :
      : [kAleToSign] "I"(kAleToSign), "r"(rom_in), "r"(instr_a_in),
        "r"(instr_b_in));

  __builtin_unreachable();
}

void romemu_setup() {
  // Claim the state machines that we will need.
  pio_sm_claim(pio, sm_a);
  pio_sm_claim(pio, sm_b);

  // Load the program into the PIO engine.
  uint prog = pio_add_program(pio, &romemu_drive_data_outputs_program);
  pio_sm_config cfg_a =
      romemu_drive_data_outputs_program_get_default_config(prog);
  pio_sm_config cfg_b =
      romemu_drive_data_outputs_program_get_default_config(prog);

  // Assign pin numbers.
  sm_config_set_jmp_pin(&cfg_a, PIN_PSEN);
  sm_config_set_jmp_pin(&cfg_b, PIN_PSEN);
  sm_config_set_set_pins(&cfg_a, PIN_AD_BASE_A, 4);
  sm_config_set_set_pins(&cfg_b, PIN_AD_BASE_B, 4);
  sm_config_set_sideset_pins(&cfg_a, PIN_AD_BASE_A);
  sm_config_set_sideset_pins(&cfg_b, PIN_AD_BASE_B);
  pio_sm_set_consecutive_pindirs(pio, sm_a, PIN_AD_BASE_A, 4, false);
  pio_sm_set_consecutive_pindirs(pio, sm_b, PIN_AD_BASE_B, 4, false);

  // Claim tristate GPIOs.
  for (int i = 0; i < 4; i++) {
    pio_gpio_init(pio, PIN_AD_BASE_A + i);
    pio_gpio_init(pio, PIN_AD_BASE_B + i);
  }

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

  // Start the state machines.
  uint pc_entry_point = prog + romemu_drive_data_outputs_offset_entry_point;
  pio_sm_init(pio, sm_a, pc_entry_point, &cfg_a);
  pio_sm_init(pio, sm_b, pc_entry_point, &cfg_b);
  pio_enable_sm_mask_in_sync(pio, (1 << sm_a) | (1 << sm_b));

  // With the state machines now running, we are now emitting NOPs (0x00) too.
  // We can tell the SN74HCT541 to stop emitting its own NOPs.
  sleep_us(100);
  gpio_put(PIN_NOPEN, 1);

  // Give SN74HCT541 extra time to fully deactivate. After this, we can emit
  // non-0x00 values without conflicting with it.
  sleep_us(100);

  // Initially fill the emulated ROM contents with 0xFF. Note that, in fact, we
  // will keep serving NOPs (0x00) until romemu_start is called.
  for (uint i = 0; i < ROM_SIZE; i++) {
    romemu_write(i, 0xFF);
  }
}

void romemu_start() {
  // Start the worker function on core1, dedicated to this task.
  multicore_launch_core1(core1_worker_task);
}

void romemu_write(uint16_t address, uint8_t value) {
  // Trasform the logical address and value into the corresponding pin-mapped
  // permutation.
  uint16_t address_pin_values = pin_map_address(address);
  uint8_t value_pin_values = pin_map_data(value);

  // Encode the PIO instruction that sets the bits managed by state machine A.
  uint16_t instr_a = pio_encode_set(pio_pins, value_pin_values & 0xF);

  // Encode the PIO instruction that sets the bits managed by state machine B.
  uint16_t instr_b = pio_encode_set(pio_pins, value_pin_values >> 4);

  // Atomically store both instructions in the rom array.
  rom[address_pin_values].store(((uint32_t)instr_b << 16) | instr_a);
}
