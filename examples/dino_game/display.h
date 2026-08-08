#ifndef PROGRAMS_DINO_DISPLAY_H
#define PROGRAMS_DINO_DISPLAY_H

#include <stdbool.h>
#include <video/commands.h>
#include <video/mcu_interface.h>

// We run in 40 columns mode, with the "double height" bit set in MAT, which
// causes all the rows except the service row to be doubled. This gives us a
// total of 1 (service row) + 12 (normal rows) = 13 rows.
#define DISPLAY_ROWS 13
#define DISPLAY_COLS 40
#define DISPLAY_WIDTH (DISPLAY_COLS * 8)  // pixels

void display_setup(void);
void display_wait_vsync(void);

void display_set_margin_color(uint8_t c);

void display_write_char(uint8_t x, uint8_t y, uint8_t a, uint8_t b, uint8_t c);
void display_write_text(uint8_t x, uint8_t y, uint8_t a, uint8_t b,
                        const char* text);

// The following two functions fill with blank characters with attribute "a".
void display_clear_screen(uint8_t a);
void display_clear_line(uint8_t y, uint8_t a);

// Copies a full row over another row, replacing its contents.
void display_copy_line(uint8_t src_y, uint8_t dst_y);

// Loads one scanline (8 monochromatic pixels) into font memory.
inline void display_load_scanline(uint8_t char_index, uint8_t y,
                                  uint8_t pixels) {
  const uint8_t d = 1, b = 0;
  uint8_t z2 = d & 1;
  uint8_t z3 = d >> 1;
  uint8_t z0 = b & 1;
  uint8_t z1 = b >> 1;

  uint8_t r4 = (char_index >> 2) | (z2 << 5);
  uint8_t r5 = (char_index & 0x03) | (y << 2) | (z0 << 7) | (z1 << 6);
  uint8_t r6 = z3 << 6;

  video_wait_busy();
  VIDEO->R1 = pixels;
  VIDEO->R4 = r4;
  VIDEO->R5 = r5;
  VIDEO->R6 = r6;
  VIDEO->ER0 = VIDEO_CMD_TBA;
}

// If building in profiling mode, the margin color is used to signal what code
// section the CPU is executing: by assigning a different color to each code
// section, it possible to visually see what the CPU was doing while a line was
// being emitted by the CRT beam.
#ifdef PROFILER
#define display_set_profiling_color(c) display_set_margin_color(c)
#else
#define display_set_profiling_color(c) \
  do {                                 \
  } while (false)
#endif

#endif
