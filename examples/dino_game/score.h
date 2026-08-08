#ifndef PROGRAMS_DINO_SCORE_H
#define PROGRAMS_DINO_SCORE_H

#include "fixed_point.h"

void score_init(void);

// Called at the beginning, during and at the end of a game.
void score_begin(void);
void score_increase(fixed_point_t rate);
bool score_end(void);

#endif
