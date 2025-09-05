#include "led.h"

#include <hardware/gpio.h>
#include <pico/binary_info.h>

#ifdef PICO_DEFAULT_LED_PIN
bi_decl(bi_1pin_with_name(PICO_DEFAULT_LED_PIN, "LED"));
#else
#include <pico/cyw43_arch.h>
#endif

void led_setup() {
#ifdef PICO_DEFAULT_LED_PIN
  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#else
  cyw43_arch_init();
#endif
}

void led_set(bool on) {
#ifdef PICO_DEFAULT_LED_PIN
  gpio_put(PICO_DEFAULT_LED_PIN, on ? 1 : 0);
#else
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on ? 1 : 0);
#endif
}
