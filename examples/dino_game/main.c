#include <8052.h>
#include <board/controls.h>
#include <board/definitions.h>
#include <keyboard/keyboard.h>
#include <stdbool.h>
#include <stdint.h>
#include <timer/timer.h>

#include "display.h"
#include "dynamic_area.h"
#include "enemies.h"
#include "fixed_point.h"
#include "floor_area.h"
#include "random.h"
#include "score.h"
#include "splash.h"
#include "static_area.h"

// Pause screen. Returns whether the user has requested to quit the game.
static bool pause_game() {
  display_write_text(14, 5, 0x70, 0x08, "PPAAUUSSEEDD");

  display_write_char(0, 12, 0x02, 0, ' ');
  display_write_text(1, 12, 0x02, 0x08, "EEsscc");
  display_write_char(7, 12, 0x02, 0, ' ');
  display_write_text(9, 12, 0x60, 0x08, "RReessuummee");

  display_write_char(27, 12, 0x02, 0, ' ');
  display_write_text(28, 12, 0x02, 0x08, "QQ");
  display_write_char(30, 12, 0x02, 0, ' ');
  display_write_text(32, 12, 0x60, 0x08, "QQuuiitt");

  // Wait for the ESC key to be released before considering further inputs.
  while (keyboard_key_is_pressed(KEY_ESCAPE)) {
  }

  while (true) {
    KEYBOARD_FOR_EACH_PRESSED_KEY(pressed_key) {
      switch (pressed_key) {
        case KEY_ESCAPE:
          // Wait for the ESC key to be released before returning to the game.
          while (keyboard_key_is_pressed(KEY_ESCAPE)) {
          }
          display_clear_line(5, 0x00);   // clear "Paused" text
          display_clear_line(12, 0x00);  // clear controls
          return false;
        case KEY_Q:
          return true;
      }
    }
  }
}

// Possible reasons for exiting run_game().
typedef enum {
  // Triggered by collisions.
  GAME_OVER,
  GAME_HIGHSCORE,

  // Explicitly requested by the user.
  GAME_QUIT
} game_exit_status_t;

// Draw a random number from a non-uniform distribution approximately centered
// around 128. This used to be a loop but it was unrolled for performance.
static inline uint8_t random_generate_around_128(void) {
  uint8_t random = 0;
  random += random_generate() % 64;
  random += random_generate() % 64;
  random += random_generate() % 64;
  random += random_generate() % 64;
  return random;
}

// Runs the game once. Returns true if a new highscore was achieved.
game_exit_status_t run_game(void) {
  // Horizontal speed of the world with respect to the dino (which never moves
  // from its fixed horizontal position).
  fixed_point_t dx = FIXED_POINT_FROM_FRAC(3, 2);

  // Vertical position and speed of the dino, simulating gravity.
  fixed_point_t y = 0;
  fixed_point_t dy = 0;
  const fixed_point_t g = -FIXED_POINT_FROM_FRAC(5, 10);

  fixed_point_t respawn_enemy_remaining_distance = 0;
  fixed_point_t respawn_rock_remaining_distance = 0;

  fixed_point_t animation = 0;
  fixed_point_t floor_x = 0;

  display_clear_screen(0x00);
  dynamic_area_init();
  static_area_init();
  floor_area_init();

  enemies_clear();
  score_begin();
  static_area_draw();

  while (true) {
    display_set_profiling_color(0);
    display_wait_vsync();

    floor_area_draw();

    // Render the dynamic area and check if the dino intersects any of the
    // enemies.
    dynamic_area_clear();
    IntersectingEnemyInfo intersection;
    dynamic_area_draw_enemies(&intersection);
    __bit hit = dynamic_area_draw_dino(FIXED_POINT_GET_INTEGRAL_PART(y),
                                       &intersection, !(animation & 0x80));

    display_set_profiling_color(7);

    if (respawn_enemy_remaining_distance < 0) {
      if (dx > FIXED_POINT_MAKE(3) && random_generate() > 200) {
        respawn_enemy_remaining_distance = FIXED_POINT_MAKE(32);
      } else {
        respawn_enemy_remaining_distance =
            20 * dx + 2 * FIXED_POINT_MAKE(random_generate_around_128());
      }

      respawn_rock_remaining_distance =
          FIXED_POINT_MAKE(random_generate_around_128());
      enemies_spawn(false);  // spawn a real enemy, not a rock
      dx += FIXED_POINT_FROM_FRAC(1, 4);
    } else if (dx <= FIXED_POINT_MAKE(9) &&
               respawn_rock_remaining_distance < 0 &&
               respawn_enemy_remaining_distance > FIXED_POINT_MAKE(128)) {
      respawn_rock_remaining_distance =
          FIXED_POINT_MAKE(random_generate_around_128());
      enemies_spawn(true);  // spawn a rock
    }

    display_set_profiling_color(4);

    respawn_enemy_remaining_distance -= dx;
    respawn_rock_remaining_distance -= dx;
    enemies_update(dx);
    floor_area_update(dx);

    display_set_profiling_color(6);

    score_increase(dx);

    if (hit) {
      break;
    }

    random_seed((y & 0xff) ^ (y >> 8));

    __bit up_pressed, esc_pressed;
#ifdef KEYBOARD_ROWS
    // This polling method is faster than KEYBOARD_FOR_EACH_PRESSED_KEY.
    up_pressed = keyboard_key_is_pressed(KEY_UP);
    esc_pressed = keyboard_key_is_pressed(KEY_ESCAPE);
#else
    // Minitels in which keyboard_key_is_pressed consumes non-matching events
    // too require the slower full-enumeration method.
    up_pressed = esc_pressed = false;
    KEYBOARD_FOR_EACH_PRESSED_KEY(pressed_key) {
      switch (pressed_key) {
        case KEY_UP:
          up_pressed = true;
          break;
        case KEY_ESCAPE:
          esc_pressed = true;
          break;
      }
    }
#endif
    if (up_pressed) {
      if (y == 0) {
        dy = FIXED_POINT_MAKE(6);
      }
    } else if (esc_pressed) {
      if (pause_game()) {
        return GAME_QUIT;
      }
    }

    y += dy;
    if (y < 0) {
      y = 0;
      dy = 0;
      animation += dx;
    }
    dy += g;
  }

  if (score_end()) {
    return GAME_HIGHSCORE;
  } else {
    return GAME_OVER;
  }
}

// Some boards require the board-specific "board_periodic_task()" function to be
// called at a fixed rate.
#ifdef BOARD_PERIODIC_TASK_HZ
static inline void board_periodic_task_reload() {
  const uint16_t reload_value = TIMER_TICKS_TO_RELOAD_VALUE_16(
      TIMER_TICKS_FROM_HZ(BOARD_PERIODIC_TASK_HZ));
  timer_adjust_thtl1(reload_value + TIMER_ADJUST_THTL_CYCLES);
}

void board_periodic_task_interrupt(void) __interrupt(TF1_VECTOR) {
  board_periodic_task_reload();
  board_periodic_task();
}

static void board_periodic_task_setup(void) {
  board_periodic_task_reload();

  // Set Timer1 in mode 1 and start it.
  TMOD = (TMOD & T0_MASK) | T1_M0;
  TR1 = 1;

  // Enable Timer1 interrupt.
  ET1 = 1;
}
#endif

void main(void) {
  display_setup();
  board_controls_set_defaults();
#ifdef BOARD_PERIODIC_TASK_HZ
  board_periodic_task_setup();
#endif
  EA = 1;

  score_init();

splash_screen:
  splash_draw();

  // Wait for SPACE to be pressed before starting the game.
  while (!keyboard_key_is_pressed(KEY_SPACE)) {
    random_seed(0);
  }

  while (true) {
  game_starting:
    // Wait for SPACE to be released before starting the game.
    while (keyboard_key_is_pressed(KEY_SPACE)) {
      random_seed(1);
    }

    // Run the game. This function returns when a collision is detected.
    game_exit_status_t status = run_game();

    // Game ended.
    switch (status) {
      case GAME_HIGHSCORE:
        display_write_text(8, 5, 0x78, 0x08, "HHIIGGHHSSCCOORREE!!!!!!");
        break;
      case GAME_OVER:
        display_write_text(4, 5, 0x78, 0x08,
                           "GG  AA  MM  EE    OO  VV  EE  RR");
        break;
      case GAME_QUIT:
        goto splash_screen;
    }

    display_write_char(0, 12, 0x02, 0, ' ');
    display_write_text(1, 12, 0x02, 0x08, "SSppaaccee");
    display_write_char(11, 12, 0x02, 0, ' ');
    display_write_text(13, 12, 0x60, 0x08, "RReettrryy");

    display_write_char(27, 12, 0x02, 0, ' ');
    display_write_text(28, 12, 0x02, 0x08, "QQ");
    display_write_char(30, 12, 0x02, 0, ' ');
    display_write_text(32, 12, 0x60, 0x08, "QQuuiitt");

    // Wait for either space to be pressed (to start a new game), or Q to return
    // to the splashscreen.
    while (true) {
      KEYBOARD_FOR_EACH_PRESSED_KEY(pressed_key) {
        switch (pressed_key) {
          case KEY_SPACE:
            goto game_starting;
          case KEY_Q:
            goto splash_screen;
        }
      }
    }
  }
}
