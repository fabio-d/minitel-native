#ifndef PROGRAMS_DINO_FIXED_POINT_H
#define PROGRAMS_DINO_FIXED_POINT_H

#include <stdint.h>

typedef int16_t fixed_point_t;

#define FIXED_POINT_BITS 4
#define FIXED_POINT_ZERO 0
#define FIXED_POINT_MAKE(integral) ((integral) << FIXED_POINT_BITS)
#define FIXED_POINT_FROM_FRAC(num, den) ((((num) << FIXED_POINT_BITS)) / (den))

#define FIXED_POINT_GET_INTEGRAL_PART(val) ((val) >> FIXED_POINT_BITS)
#define FIXED_POINT_GET_FRACTIONAL_PART(val) \
  ((val) & ((1 << FIXED_POINT_BITS) - 1))

#endif
