#include <8052.h>

#include "board/definitions.h"

#define SHIFTREG_DATA_BIT_7 P1_6
#define SHIFTREG_CLOCK P1_5
#define SHIFTREG_PARALLEL_LOAD P1_4

uint8_t board_read_keyboard(uint8_t row) {
  // Lines P0:P3 select the row.
  P1_0 = !!(row & 1);
  P1_1 = !!(row & 2);
  P1_2 = !!(row & 4);
  P1_3 = !!(row & 8);

  // Latch in shift register.
  SHIFTREG_PARALLEL_LOAD = 1;
  SHIFTREG_CLOCK = 0;
  SHIFTREG_CLOCK = 1;
  SHIFTREG_PARALLEL_LOAD = 0;

  // Shift bits in.
  uint8_t result = SHIFTREG_DATA_BIT_7;
  for (int i = 0; i < 7; ++i) {
    SHIFTREG_CLOCK = 0;
    SHIFTREG_CLOCK = 1;
    result = (result << 1) | SHIFTREG_DATA_BIT_7;
  }

  return result;
}
