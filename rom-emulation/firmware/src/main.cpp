#include <hardware/structs/busctrl.h>
#include <pico/binary_info.h>
#include <pico/stdlib.h>
#include <stdio.h>
#include <stdlib.h>

#include "embedded-rom-array.h"
#include "led.h"
#include "romemu.h"

bi_decl(bi_program_feature(MINITEL_MODEL_FEATURE));
bi_decl(bi_program_feature(OPERATING_MODE_FEATURE));

static_assert(sizeof(EMBEDDED_ROM) <= MAX_ROM_SIZE);

int main() {
  // Give core1 (the CPU that will serve the ROM) priority access to the RAM,
  // so that it's never stalled.
  busctrl_hw->priority = BUSCTRL_BUS_PRIORITY_PROC1_BITS;

  // Take over the duty of responding to PSEN requests from the SN74HCT541 to
  // ourselves.
  romemu_setup();

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

    // Just echo back what we receive, for now.
    uint32_t r = stdio_getchar_timeout_us(0);
    if (r != PICO_ERROR_TIMEOUT) {
      printf("Received %02x from stdin!\n", r);
    }
  }
}
