#include "board/controls.h"

#include <8052.h>

void board_controls_set_defaults(void) {
  P1_0 = 1;  // not in standby
}
