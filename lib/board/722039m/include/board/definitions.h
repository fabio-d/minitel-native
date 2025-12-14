#ifndef LIB_BOARD_722039M_INCLUDE_BOARD_DEFINITIONS_H
#define LIB_BOARD_722039M_INCLUDE_BOARD_DEFINITIONS_H

#include <stdbool.h>
#include <stdint.h>

#define XTAL_HZ 12000000

#define VIDEO_EF9345
#define VIDEO_MCU_INTERFACE_BASE_ADDRESS 0x6020
#define VIDEO_TGS_BOARD_EXTRAS VIDEO_TGS_CSYNC_OUT_NP
#define VIDEO_PAT_BOARD_EXTRAS 0

#define BOARD_PERIODIC_TASK_HZ 1200
void board_periodic_task(void);

bool board_read_keyboard_raw_stream(uint8_t* dest);
uint8_t board_read_keyboard_key(void);
uint8_t board_read_keyboard_modifier(void);

#define KEYBOARD_FOR_EACH_PRESSED_KEY(key)               \
  for (uint8_t ___key2 = board_read_keyboard_modifier(), \
               key = board_read_keyboard_key();          \
       key != 0 || ___key2 != 0; key = ___key2, ___key2 = 0)

#endif
