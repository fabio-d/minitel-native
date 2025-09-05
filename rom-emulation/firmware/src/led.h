#ifndef ROM_EMULATION_FIRMWARE_SRC_LED_H
#define ROM_EMULATION_FIRMWARE_SRC_LED_H

#include <stdbool.h>

// Initializes the Pico's LED and set it to on.
void led_setup();

// Set the LED value.
void led_set(bool on);

#endif
