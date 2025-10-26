#include "trace.h"

#include <hardware/pio.h>
#include <pico/time.h>

#include "pin-map.h"
#include "trace.pio.h"

// The PIO program requires PSEN and ALE to be consecutive, because it uses them
// as a 2-bit index in a jump table.
static_assert(PIN_PSEN == PIN_ALE + 1, "PSEN and ALE must be consecutive");

static const PIO pio = pio2;
static constexpr uint sm = 0;

void trace_setup() {
  // Claim the state machine.
  pio_sm_claim(pio, sm);

  // Load the program into the PIO engine.
  uint prog = pio_add_program(pio, &trace_ale_then_psen_program);
  pio_sm_config cfg = trace_ale_then_psen_program_get_default_config(prog);

  // Configure input pin rotation so that PSEN and ALE are the two rightmost
  // bits.
  sm_config_set_in_pin_base(&cfg, PIN_ALE);

  // Start the state machine.
  uint pc_entry_point = prog + trace_ale_then_psen_offset_entry_point;
  pio_sm_init(pio, sm, pc_entry_point, &cfg);
  pio_sm_set_enabled(pio, sm, true);
}

uint trace_collect(uint max_samples, absolute_time_t deadline, uint16_t *buf) {
  uint count = 0;

  // Discard any enqueued old data.
  pio_sm_clear_fifos(pio, sm);

  // Collect fresh samples.
  while (absolute_time_diff_us(deadline, get_absolute_time()) < 0 &&
         count < max_samples) {
    if (!pio_sm_is_rx_fifo_empty(pio, sm)) {
      // Counter the rotation due to:
      // - sm_config_set_in_pin_base (PIN_ALE bits right)
      // - the PIO program itself (2 more bits right)
      buf[count++] = pio_sm_get(pio, sm) >> (32 - PIN_ALE - 2);
    }
  }

  // Compiler barrier to ensure that the slow code below doesn't get moved into
  // the performance-critical loop above.
  asm volatile("" ::: "memory");

  for (uint i = 0; i < count; i++) {
    buf[i] = pin_map_address_inverse(buf[i]);
  }

  return count;
}
