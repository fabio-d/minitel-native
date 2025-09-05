#ifndef LIB_VIDEO_INCLUDE_VIDEO_MCU_INTERFACE_H
#define LIB_VIDEO_INCLUDE_VIDEO_MCU_INTERFACE_H

#include <stdint.h>

#include "board/definitions.h"

typedef struct {
  volatile uint8_t R0;
  volatile uint8_t R1;
  volatile uint8_t R2;
  volatile uint8_t R3;
  volatile uint8_t R4;
  volatile uint8_t R5;
  volatile uint8_t R6;
  volatile uint8_t R7;
  volatile uint8_t ER0;
  volatile uint8_t ER1;
  volatile uint8_t ER2;
  volatile uint8_t ER3;
  volatile uint8_t ER4;
  volatile uint8_t ER5;
  volatile uint8_t ER6;
  volatile uint8_t ER7;
} VIDEO_t;

#define VIDEO ((__xdata VIDEO_t*)VIDEO_MCU_INTERFACE_BASE_ADDRESS)

inline void video_wait_busy() {
  while (VIDEO->R0 & 0x80) {
  }
}

#endif
