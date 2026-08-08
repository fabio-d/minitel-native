#ifndef PROGRAMS_DINO_STATIC_AREA_H
#define PROGRAMS_DINO_STATIC_AREA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The "static area" is the area of the screen that do not change while playing,
// i.e. the background scenery.
//
// To simplify rendering, the static area is fully disjoint from the dynamic
// area (no overlaps are possible).

// Loads the necessary resources into video RAM.
void static_area_init(void);

// Draws the elements of the static area.
void static_area_draw(void);

#endif
