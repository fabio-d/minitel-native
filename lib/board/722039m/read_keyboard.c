#include <8052.h>

#include "board/definitions.h"

#define TVP P1_1  // from CPU to keyboard assembly
#define TPV P1_2  // from keyboard assembly to CPU

#define START_VALUE 0x2b
#define IDLE_VALUE 0x4d

#define PARITY(val)                                                   \
  !(((val >> 0) ^ (val >> 1) ^ (val >> 2) ^ (val >> 3) ^ (val >> 4) ^ \
     (val >> 5) ^ (val >> 6) ^ (val >> 7)) &                          \
    1)

static int16_t phase = -2 * BOARD_PERIODIC_TASK_HZ;  // 2 s initial delay

static uint8_t txbuf = START_VALUE;
static __bit txbuf_parity;
static uint8_t rxbuf;
static __bit rxbuf_start, rxbuf_parity;

static __bit rawval_is_present = false;
static uint16_t rawval;

static enum decode_state_t {
  DECODE_STATE_IDLE,
  DECODE_STATE_LINE_STATUS,
} decode_state;

static __bit report_key_consumed = true;
static uint8_t report_key, report_modifier;

#pragma save
#pragma nooverlay
void board_periodic_task(void) {
  switch (phase++) {
    case 0:  // TX: start bit
      TVP = 0;
      txbuf_parity = 1;
      break;
    case 2:  // TX: least significant bit
    case 4:
    case 6:
    case 8:
    case 10:
    case 12:
    case 14:
    case 16:  // TX: most significant bit
      if (txbuf & 1) {
        TVP = 1;
        txbuf_parity ^= 1;
      } else {
        TVP = 0;
      }
      txbuf >>= 1;
      break;
    case 18:  // TX: parity bit
      TVP = txbuf_parity;
      break;
    case 20:  // TX: stop bit
      TVP = 1;
      break;
    case 1:  // RX: start bit
      rxbuf_start = TPV;
      rxbuf_parity = 1;
      break;
    case 3:  // RX: least significant bit
    case 5:
    case 7:
    case 9:
    case 11:
    case 13:
    case 15:
    case 17:  // RX: most significant bit
      if (TPV) {
        rxbuf = (rxbuf >> 1) | 0x80;
        rxbuf_parity ^= 1;
      } else {
        rxbuf >>= 1;
      }
      break;
    case 19:  // RX: parity
      if (rxbuf_start == 0 && TPV == rxbuf_parity) {
        __bit is_idle = false;
        __bit is_key = false;
        __bit is_key_repeat = false;
        __bit is_modifier = false;
        __bit is_line_status = false;

        switch (decode_state) {
          case DECODE_STATE_IDLE:
            switch (rxbuf) {
              case IDLE_VALUE:
                is_idle = true;
                break;
              case 0xE9:
                is_key_repeat = true;
                break;
              case 0xEA:
                decode_state = DECODE_STATE_LINE_STATUS;
                break;
              case 0xE0:
              case 0xE3:
              case 0xE5:
              case 0xF8:
                is_modifier = true;
                break;
              default:
                is_key = true;
                break;
            }
            break;
          case DECODE_STATE_LINE_STATUS:
            is_line_status = true;
            decode_state = DECODE_STATE_IDLE;
            break;
        }

        if (!is_idle) {
          __critical {
            rawval_is_present = true;
            rawval = rxbuf;

            if (is_key) {
              report_key = rxbuf;
              report_key_consumed = false;
            }
            if (is_key_repeat) {
              report_key_consumed = false;
            }
            if (is_modifier) {
              report_modifier = rxbuf;
            }
          }
        }
      }
      break;
    case 47:  // restart on next cycle (matching the stock ROM's poll interval)
      phase = 0;

      txbuf = IDLE_VALUE;
      break;
  }
}
#pragma restore

bool board_read_keyboard_raw_stream(__data uint8_t* dest) {
  bool result;

  __critical {
    *dest = rawval;
    result = rawval_is_present;
    rawval_is_present = false;
  }

  return result;
}

uint8_t board_read_keyboard_key(void) {
  uint8_t result;

  __critical {
    if (!report_key_consumed) {
      result = report_key;
      report_key_consumed = true;
    } else {
      result = 0;
    }
  }

  return result;
}

uint8_t board_read_keyboard_modifier(void) { return report_modifier; }
