#include "board/controls.h"

#include <8052.h>
#include <stdint.h>

volatile __xdata __at(0x2000) uint8_t CTRL_LATCH;

void board_controls_set_defaults(void) {
  CTRL_LATCH = 0x08;  // Turn on CRT.
}
