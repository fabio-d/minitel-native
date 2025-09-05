#ifndef PROGRAMS_HELLO_WORLD_DISPLAY_H
#define PROGRAMS_HELLO_WORLD_DISPLAY_H

#include <stdint.h>

void display_setup(void);

void clear_line(uint8_t y);
void clear_full(void);

void draw_string(uint8_t x, uint8_t y, const char *text, uint8_t attributes);
void draw_rectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
void draw_color_pattern(uint8_t x0, uint8_t y0);

#endif
