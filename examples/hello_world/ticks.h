#ifndef PROGRAMS_HELLO_WORLD_INCLUDE_TICKS_H
#define PROGRAMS_HELLO_WORLD_INCLUDE_TICKS_H

#include <8052.h>

void ticks_setup(void);

unsigned long ticks_get();  // 1 tick = 1 ms

// SDCC manual 3.8.1: "If you have multiple source files in your project,
// interrupt service routines can be present in any of them, but a prototype of
// the isr MUST be present or included in the file that contains the function
// main".
void ticks_interrupt(void) __interrupt(TF0_VECTOR);

#endif
