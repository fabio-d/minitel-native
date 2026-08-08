#ifndef PROGRAMS_DINO_FLOOR_AREA_H
#define PROGRAMS_DINO_FLOOR_AREA_H

#include "dynamic_area.h"
#include "fixed_point.h"

// The "floor area" is the area of the screen right below where the dino walks.
#define FLOOR_ROW (ENEMY_BASE_ROW + 1)

void floor_area_init(void);
void floor_area_draw(void);
void floor_area_update(fixed_point_t dino_speed);

#endif
