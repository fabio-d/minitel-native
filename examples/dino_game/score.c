#include "score.h"

#include "display.h"

#define NUM_DIGITS 5
static  __idata uint8_t score_digits[NUM_DIGITS];
static  __idata uint8_t highscore_digits[NUM_DIGITS];
static uint16_t accumulator;

#define HIGHSCORE_COL sizeof(HIGHSCORE_TEXT)
static __code char HIGHSCORE_TEXT[] = "HIGHSCORE";

#define SCORE_COL (DISPLAY_COLS - NUM_DIGITS)
static __code char SCORE_TEXT[] = "SCORE";

static inline void score_draw(uint8_t x, __idata uint8_t* digits_bcd) {
  int score_x = x + NUM_DIGITS;
  for (uint8_t i = 0; i < NUM_DIGITS; ++i) {
    display_write_char(--score_x, 0, 0x70, 0, digits_bcd[i]);
  }
}

// Tests if BCD-encoded score A is strictly less than B.
static bool score_less_than(__idata uint8_t* digits_bcd_a,
                            __idata uint8_t* digits_bcd_b) {
  uint8_t i = NUM_DIGITS;
  while (i-- != 0) {
    if (digits_bcd_a[i] != digits_bcd_b[i]) {
      return digits_bcd_a[i] < digits_bcd_b[i];
    }
  }

  return false;  // equal
}

void score_init(void) {
  for (uint8_t i = 0; i < NUM_DIGITS; ++i) {
    highscore_digits[i] = '0';
  }
}

void score_begin(void) {
  // Draw the highscore.
  display_write_text(0, 0, 0x50, 0, HIGHSCORE_TEXT);
  score_draw(HIGHSCORE_COL, highscore_digits);

  // Reset the current score to zero.
  for (uint8_t i = 0; i < NUM_DIGITS; ++i) {
    score_digits[i] = '0';
  }
  accumulator = 0;

  // Draw the current score.
  display_write_text(DISPLAY_COLS - NUM_DIGITS - sizeof(SCORE_TEXT), 0, 0x50, 0,
                     SCORE_TEXT);
  score_draw(SCORE_COL, score_digits);
}

void score_increase(fixed_point_t rate) {
  uint8_t bcd_carry_digit = 0;

  accumulator += rate;
  if (accumulator >= 256) {
    bcd_carry_digit = accumulator / 256;
    if (bcd_carry_digit > 9) {
      bcd_carry_digit = 9;
      accumulator = 0;
    } else {
      accumulator %= 256;
    }
  }

  for (uint8_t i = 0; i < NUM_DIGITS; ++i) {
    score_digits[i] += bcd_carry_digit;
    if (score_digits[i] > '9') {
      score_digits[i] -= 10;
      bcd_carry_digit = 1;
    } else {
      bcd_carry_digit = 0;
    }
  }

  score_draw(SCORE_COL, score_digits);
}

bool score_end(void) {
  if (score_less_than(highscore_digits, score_digits)) {
    // Store and draw the new highscore.
    for (uint8_t i = 0; i < NUM_DIGITS; ++i) {
      highscore_digits[i] = score_digits[i];
    }
    score_draw(HIGHSCORE_COL, highscore_digits);

    return true;
  } else {
    return false;  // Not a highscore
  }
}
