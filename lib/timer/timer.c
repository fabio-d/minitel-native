#include "timer/timer.h"

#include <8052.h>

// clang-format off
#define TIMER_ADJUST_THTL_IMPL(funcname, TRx, THx, TLx)                        \
  void funcname(uint16_t ticks) __naked {                                      \
    (void)ticks;                                                               \
    __asm__(                                                                   \
        /* Temporarily pause the timer. */                                     \
        "clr "TRx"\n"                                                          \
                                                                               \
        /* Add low byte to TLx. */                                             \
        "mov A, "TLx" ; 1 cycle\n"                                             \
        "add A, DPL   ; 1 cycle\n"                                             \
        "mov "TLx", A ; 1 cycle\n"                                             \
                                                                               \
        /* Add high byte to THx. */                                            \
        "mov A, "THx" ; 1 cycle\n"                                             \
        "addc A, DPH  ; 1 cycle\n"                                             \
        "mov "THx", A ; 1 cycle\n"                                             \
                                                                               \
        /* Resume the timer and return. */                                     \
        "setb "TRx"   ; 1 cycle\n"                                             \
        "ret\n");                                                              \
  }
// clang-format on

TIMER_ADJUST_THTL_IMPL(timer_adjust_thtl0, "TR0", "TH0", "TL0")
TIMER_ADJUST_THTL_IMPL(timer_adjust_thtl1, "TR1", "TH1", "TL1")
