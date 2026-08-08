#ifndef PROGRAMS_DINO_ENEMIES_H
#define PROGRAMS_DINO_ENEMIES_H

#include <stdbool.h>
#include <stdint.h>

#include "fixed_point.h"

typedef enum {
  // Real enemies.
  ENEMY_NONE,
  ENEMY_CACTUS,
  ENEMY_BIRD,

  // Not real enemies, they are spawned and rendered but do not generate
  // collisions.
  ENEMY_ROCK1,
  ENEMY_ROCK2,
} EnemyType;

typedef struct {
  EnemyType type : 4;
  uint8_t row : 4;  // 0=bottom, 1=middle, 2=top
  fixed_point_t x;
} Enemy;

#define ENEMIES_SIZE 7
extern Enemy enemies[ENEMIES_SIZE];

void enemies_clear(void);
void enemies_spawn(bool rock);
void enemies_update(fixed_point_t dino_speed);

#endif
