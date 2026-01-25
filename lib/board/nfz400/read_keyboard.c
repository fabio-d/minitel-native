#include <8052.h>

#include "board/definitions.h"

#define SHIFTREG_DATA_BIT_7 P1_0
#define SHIFTREG_PARALLEL_LOAD P1_5

// Reads into region 0x2000-0x2FFF send a clock pulse to the shift register.
//
// While loading the keyboard state, lines A8-A11 select the row to be scanned.
// Note:
//  - only 9 rows exist in the keyboard matrix.
//  - the order of the 4 bits in the row index is reversed.
static const uint8_t ADDR_TOP[] = {
    0x20 | 0b0000,  // row 0
    0x20 | 0b1000,  // row 1
    0x20 | 0b0100,  // row 2
    0x20 | 0b1100,  // row 3
    0x20 | 0b0010,  // row 4
    0x20 | 0b1010,  // row 5
    0x20 | 0b0110,  // row 6
    0x20 | 0b1110,  // row 7
    0x20 | 0b0001,  // row 8
};

uint8_t board_read_keyboard(uint8_t row) {
  // Assemble the trigger pointer:
  // - the high byte selects the requested row.
  // - the low byte is ignored by the hardware.
  volatile __xdata uint8_t* trig =
      (volatile __xdata uint8_t*)(ADDR_TOP[row] << 8);

  // Latch the row's state into the shift register.
  SHIFTREG_PARALLEL_LOAD = 1;
  uint8_t dummy = *trig;
  SHIFTREG_PARALLEL_LOAD = 0;

  // Shift bits in. Since data has already been latched, lines A8-A11 of the
  // trigger address are ignored.
  uint8_t result = SHIFTREG_DATA_BIT_7;
  for (int i = 0; i < 7; ++i) {
    dummy = *trig;
    result = (result << 1) | SHIFTREG_DATA_BIT_7;
  }

  return result;
}
