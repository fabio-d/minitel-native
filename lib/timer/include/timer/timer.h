#ifndef LIB_TIMER_INCLUDE_TIMER_TIMER_H
#define LIB_TIMER_INCLUDE_TIMER_TIMER_H

#include <stdint.h>

#include "board/definitions.h"

// Internal macro for asserting that the `value` argument is:
// - known at compile time
// - within the given range (`min` <= `value` <= `max`)
// If these conditions do not hold, a reference to a non-existent symbol will be
// generated and linking will fail.
extern char TIMER___STATIC_ASSERT_BOUNDS_FAILED;  // never defined anywhere
#define TIMER___STATIC_ASSERT_BOUNDS(value, min, max)                         \
  (((value) < (min) || (value) > (max)) ? TIMER___STATIC_ASSERT_BOUNDS_FAILED \
                                        : (value))

// Internal macro for rounding a fixed-point number (with 1 fractional bit) to
// the closest integer.
#define TIMER___ROUND_FIXED_POINT_1(value) (((value) >> 1) + ((value) & 1))

// Transforms an interval in microseconds or a frequency in Hz into the
// corresponding number of timer ticks. These formulas are usable for all timers
// (0, 1 and 2), except for Timer 2 in clock-out and baud-rate generator modes.
#define TIMER_TICKS_FROM_US(us) \
  TIMER___ROUND_FIXED_POINT_1((long long)(us) * XTAL_HZ / 1000000 / 6)
#define TIMER_TICKS_FROM_HZ(hz) \
  TIMER___ROUND_FIXED_POINT_1(XTAL_HZ / ((long long)(hz) * 6))

// These are the corresponding formulas for Timer 2 in clock-out mode. Note that
// these formulas already take the postscaler by 2 into account.
#define TIMER_TICKS_FROM_US_FOR_T2_CLOCKOUT(us) \
  TIMER___ROUND_FIXED_POINT_1((long long)(us) * XTAL_HZ / 1000000 / 2)
#define TIMER_TICKS_FROM_HZ_FOR_T2_CLOCKOUT(hz) \
  TIMER___ROUND_FIXED_POINT_1(XTAL_HZ / ((long long)(hz) * 2))

// Computes reload values suitable for configuring the serial port in Mode 1 and
// Mode 3, using either Timer 1 (with and without the SMOD bit set) and Timer 2.
#define TIMER_TICKS_FROM_BAUD_T1_SMOD0(baud) \
  TIMER_TICKS_FROM_HZ(((long long)(baud) * 32))
#define TIMER_TICKS_FROM_BAUD_T1_SMOD1(baud) \
  TIMER_TICKS_FROM_HZ(((long long)(baud) * 16))
#define TIMER_TICKS_FROM_BAUD_T2(baud) \
  TIMER___ROUND_FIXED_POINT_1(XTAL_HZ / ((long long)(baud) * 16))

// Computation of the reload value for 8 bit timers.
#define TIMER_TICKS_TO_RELOAD_VALUE_8(ticks) \
  ((uint8_t)(-TIMER___STATIC_ASSERT_BOUNDS((ticks), 1LL, 0x100LL)))

// Computation of the reload value for 16 bit timers.
#define TIMER_TICKS_TO_RELOAD_VALUE_16(ticks) \
  ((uint16_t)(-TIMER___STATIC_ASSERT_BOUNDS((ticks), 1LL, 0x10000LL)))

// Perform a 16-bit addition to TH0:TL0 or TH1:TL1 while temporarily pausing the
// respective timer for TIMER_ADJUST_THTL_CYCLES cycles.
//
// This function must be called with interrupts disabled.
void timer_adjust_thtl0(uint16_t ticks) __naked;
void timer_adjust_thtl1(uint16_t ticks) __naked;
#define TIMER_ADJUST_THTL_CYCLES 7

#endif
