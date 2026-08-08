#ifndef PROGRAMS_DINO_DYNAMIC_AREA_H
#define PROGRAMS_DINO_DYNAMIC_AREA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The "dynamic area" is the area of the screen that changes as a result of the
// gameplay. It covers all the tiles in which the dino might be and all the
// tiles in which the enemies might be.
//
// The bottom row of the dynamic area is ENEMY_BASE_ROW, which represents ground
// level. The enemies can be anywhere up to ENEMY_AREA_ROWS above it.
//
// The dino can only jump vertically from ground level to DINO_AREA_ROWS rows
// above. Horizontally, it never moves and it always occupies DINO_AREA_COLS (2)
// horizontally-adjacent tiles, starting from column DINO_LEADING_COL.
//
// To simplify rendering, the static area is fully disjoint from the dynamic
// area (no overlaps are possible).

#define ENEMY_BASE_ROW 10
#define ENEMY_AREA_ROWS 3
#define DINO_LEADING_COL 10
#define DINO_AREA_COLS 2
#define DINO_AREA_ROWS 5

typedef struct {
  __code const uint8_t* sprites[DINO_AREA_COLS];  // NULL = no intersection
  int8_t shifts[DINO_AREA_COLS];
  uint8_t rows[DINO_AREA_COLS];
} IntersectingEnemyInfo;

// Loads the necessary resources into video RAM.
void dynamic_area_init(void);

// The following three functions must be called in this order to render one
// frame, passing the same "intersection" object from one to the other:
// - dynamic_area_clear: prepares for drawing by clearing all the tiles
//   belonging to the dynamic area (to remove what was left by the previous
//   frame).
// - dynamic_area_draw_enemies: draws all the sprites falling outside of the
//   dino's two columns; sprites that fall in the dino's two columns, instead,
//   are simply recorded in the output "intersection" array.
// - dynamic_area_draw_dino: draws the dino and the sprites listed in the
//   "intersection" array.
// If, while drawing, dynamic_area_draw_dino detects that the dino has collided
// with one of the sprites in "intersection", it returns true. Otherwise, if
// there are no intersections, it returns false.
void dynamic_area_clear(void);
void dynamic_area_draw_enemies(__data IntersectingEnemyInfo* intersection);
bool dynamic_area_draw_dino(uint8_t y,
                            __data const IntersectingEnemyInfo* intersection,
                            bool feet_toggle);

#endif
