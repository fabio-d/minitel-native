#include <8052.h>

#include "board/definitions.h"

// Read accesses to this region send a clock pulse to the shift register.
static volatile __xdata __at(0x2000) uint8_t KEYB_TRIG_AREA[16 << 8];

#define SHIFTREG_DATA_BIT_7 P1_0
#define SHIFTREG_PARALLEL_LOAD P1_5

uint8_t board_read_keyboard(uint8_t row) {
  // Reverse the order of the 4 bits in the row index.
  uint8_t trig_offset = 0;
  if (row & 1) trig_offset |= 8;
  if (row & 2) trig_offset |= 4;
  if (row & 4) trig_offset |= 2;
  if (row & 8) trig_offset |= 1;

  // Latch in shift register, lines A8-A11 (in reversed bit order!) select the
  // row.
  SHIFTREG_PARALLEL_LOAD = 1;
  uint8_t dummy = KEYB_TRIG_AREA[trig_offset << 8];
  SHIFTREG_PARALLEL_LOAD = 0;

  // Shift bits in. Since data has already been latched, lines A8-A11 are
  // ignored.
  uint8_t result = SHIFTREG_DATA_BIT_7;
  for (int i = 0; i < 7; ++i) {
    dummy = KEYB_TRIG_AREA[0];
    result = (result << 1) | SHIFTREG_DATA_BIT_7;
  }

  return result;
}
