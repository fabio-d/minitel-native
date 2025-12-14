#ifndef PROGRAMS_HELLO_WORLD_KEYB_H
#define PROGRAMS_HELLO_WORLD_KEYB_H

#include <stdint.h>

// Draws the static part of the keyboard widget at the given coordinates.
//
// The static part consists of the row/column headers and the enclosing
// rectangle.
void keyb_widget_prepare(uint8_t x0, uint8_t y0);

// Draws the dynamic part of the keyboard widget at the given coordinates.
//
// The dynamic part consists of the state of all the keys and the name of the
// key that is currently pressed, if any.
void keyb_widget_fill(uint8_t x0, uint8_t y0);

#endif
