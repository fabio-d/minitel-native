#include <8052.h>
#include <board/controls.h>
#include <stdbool.h>
#include <stdio.h>

#include "display.h"
#include "keyb.h"
#include "ticks.h"

void main(void) {
  ticks_setup();

  display_setup();
  clear_full();

  board_controls_set_defaults();

  // Print "Hello World!" in double-height, double-width, flashing, white color.
  draw_string(8, 2, "HHeelllloo  WWoorrlldd!!", 0x3F);
  draw_string(8, 3, "HHeelllloo  WWoorrlldd!!", 0x3F);

  // Print color matrix.
  draw_color_pattern(0, 5);

  // Print skeleton for keyboard matrix.
  keyb_matrix_prepare(21, 5);

  // More example texts showcasing text sizing.
  draw_string(0, 21, "Normal Size", 0x07);
  draw_string(16, 21, "DDoouubbllee  Wwiiddtthh", 0x27);
  draw_string(0, 23, "Double Height", 0x17);
  draw_string(0, 24, "Double Height", 0x17);
  draw_string(16, 23, "DDoouubbllee  WW  &&  HH", 0x37);
  draw_string(16, 24, "DDoouubbllee  WW  &&  HH", 0x37);

  // Start serving interrupts.
  EA = 1;

  while (true) {
    // Print the number of milliseconds since boot.
    char buf[16];
    sprintf(buf, "%lu ms", ticks_get());
    draw_string(0, 0, buf, 0x07);

    // Print the current keyboard state.
    keyb_matrix_fill(21, 5);
  }
}
