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
static uint16_t rxbuf;

static bool rawval_is_present = false;
static uint16_t rawval;

static enum decode_state_t {
  DECODE_STATE_IDLE,
  DECODE_STATE_LINE_STATUS,
} decode_state;

static bool report_key_consumed = true;
static uint8_t report_key, report_modifier;

void board_periodic_task(void) {
  switch (phase++) {
    case 0:
      TVP = 0;  // start bit
      break;
    case 2:
      TVP = !!(txbuf & (1 << 0));  // least significant bit
      break;
    case 4:
      TVP = !!(txbuf & (1 << 1));
      break;
    case 6:
      TVP = !!(txbuf & (1 << 2));
      break;
    case 8:
      TVP = !!(txbuf & (1 << 3));
      break;
    case 10:
      TVP = !!(txbuf & (1 << 4));
      break;
    case 12:
      TVP = !!(txbuf & (1 << 5));
      break;
    case 14:
      TVP = !!(txbuf & (1 << 6));
      break;
    case 16:
      TVP = !!(txbuf & (1 << 7));  // most significant bit
      break;
    case 18:
      TVP = PARITY(txbuf);  // parity bit
      break;
    case 20:
      TVP = 1;  // stop bit
      break;
    case 1:  // start bit
      rxbuf = (TPV ? 0x100 : 0);
      break;
    case 3:
    case 5:
    case 7:
    case 9:
    case 11:
    case 13:
    case 15:
    case 17:
      rxbuf = (rxbuf >> 1) | (TPV ? 0x100 : 0);
      break;
    case 19:
      if ((rxbuf & 1) == 0) {  // was the start bit 0?
        rxbuf >>= 1;
        if (PARITY(rxbuf) == TPV) {  // is the parity bit correct?
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
      }
      break;
    case 47:  // restart on next cycle (matching the stock ROM's poll interval)
      phase = 0;

      txbuf = IDLE_VALUE;
      break;
  }
}

bool board_read_keyboard_raw_stream(uint8_t* dest) {
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
