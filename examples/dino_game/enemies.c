#include "enemies.h"

#include "display.h"
#include "dynamic_area.h"
#include "random.h"

Enemy enemies[ENEMIES_SIZE];

void enemies_clear(void) {
  for (uint8_t i = 0; i < ENEMIES_SIZE; ++i) {
    enemies[i].type = ENEMY_NONE;
  }
}

void enemies_spawn(bool rock) {
  for (uint8_t i = 0; i < ENEMIES_SIZE; ++i) {
    if (enemies[i].type == ENEMY_NONE) {
      if (rock) {
        enemies[i].type = random_generate() < 128 ? ENEMY_ROCK1 : ENEMY_ROCK2;
        enemies[i].row = 0;
      } else {
        enemies[i].type = random_generate() < 200 ? ENEMY_CACTUS : ENEMY_BIRD;
        enemies[i].row = enemies[i].type == ENEMY_BIRD
                             ? (random_generate() % ENEMY_AREA_ROWS)
                             : 0;
      }
      enemies[i].x = FIXED_POINT_MAKE(DISPLAY_WIDTH - 1);
      break;
    }
  }
}

void enemies_update(fixed_point_t dino_speed) {
  for (uint8_t i = 0; i < ENEMIES_SIZE; ++i) {
    if (enemies[i].type != ENEMY_NONE) {
      enemies[i].x -= dino_speed;
      if (enemies[i].x < -FIXED_POINT_MAKE(7)) {
        enemies[i].type = ENEMY_NONE;
      }
    }
  }
}
