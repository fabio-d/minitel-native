#ifndef LIB_TIMER_INCLUDE_TIMER_TIMER_H
#define LIB_TIMER_INCLUDE_TIMER_TIMER_H

#include <stdint.h>

#include "board/definitions.h"

// Internal macros for asserting that the `value` argument is:
// - known at compile time
// - within the given range (`min` <= `value` <= `max`)
// If these conditions do not hold, a reference to a non-existent symbol will be
// generated and linking will fail.
extern char TIMER___STATIC_ASSERT_BOUNDS_FAILED;  // never defined anywhere
#define TIMER___STATIC_ASSERT_BOUNDS(value, min, max)                         \
  (((value) < (min) || (value) > (max)) ? TIMER___STATIC_ASSERT_BOUNDS_FAILED \
                                        : (value))

// Transforms an interval in microseconds or a frequency in Hz into the
// corresponding number of timer ticks. These formulas are usable for all timers
// (0, 1 and 2), except for Timer 2 in clock-out and baud-rate generator modes.
#define TIMER_TICKS_FROM_US(us) ((long long)(us) * XTAL_HZ / 1000000 / 12)
#define TIMER_TICKS_FROM_HZ(hz) (XTAL_HZ / ((long long)(hz) * 12))

// These are the corresponding formulas for Timer 2 in clock-out mode. Note that
// these formulas already take the postscaler by 2 into account.
#define TIMER_TICKS_FROM_US_FOR_T2_CLOCKOUT(us) \
  ((long long)(us) * XTAL_HZ / 1000000 / 4)
#define TIMER_TICKS_FROM_HZ_FOR_T2_CLOCKOUT(hz) \
  (XTAL_HZ / ((long long)(hz) * 4))

// Computes reload values suitable for configuring the serial port in Mode 1 and
// Mode 3, using either Timer 1 (with and without the SMOD bit set) and Timer 2.
#define TIMER_TICKS_FROM_BAUD_T1_SMOD0(baud) \
  TIMER_TICKS_FROM_HZ(((long long)(baud) * 32))
#define TIMER_TICKS_FROM_BAUD_T1_SMOD1(baud) \
  TIMER_TICKS_FROM_HZ(((long long)(baud) * 16))
#define TIMER_TICKS_FROM_BAUD_T2(baud) (XTAL_HZ / ((long long)(baud) * 32))

// Computation of the reload value for 8 bit timers.
#define TIMER_TICKS_TO_RELOAD_VALUE_8(ticks) \
  ((uint8_t)(-TIMER___STATIC_ASSERT_BOUNDS((ticks), 1LL, 0x100LL)))

// Computation of the reload value for 16 bit timers.
#define TIMER_TICKS_TO_RELOAD_VALUE_16(ticks) \
  ((uint16_t)(-TIMER___STATIC_ASSERT_BOUNDS((ticks), 1LL, 0x10000LL)))

#endif
