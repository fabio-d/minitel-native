#ifndef LIB_VIDEO_INCLUDE_VIDEO_REGISTERS_H
#define LIB_VIDEO_INCLUDE_VIDEO_REGISTERS_H

#include "board/definitions.h"

// "Portable" settings work the same way on both chips. They are safe to use in
// programs targeting both.
//
// "Non portable" settings (ending with "_NP") do not exist or are different in
// the other chip.

// Text mode configuration.
// Warning: On the EF9345, it is split among TGS and PAT. In portable programs,
// always use both parts when constructing values for TGS and PAT!
#if defined(VIDEO_EF9345)
#define VIDEO_TGS_MODE_40L (0b00 << 6)
#define VIDEO_PAT_MODE_40L 0
#define VIDEO_TGS_MODE_40V_NP (0b01 << 6)
#define VIDEO_PAT_MODE_40V_NP 0
#define VIDEO_TGS_MODE_40S (0b00 << 6)
#define VIDEO_PAT_MODE_40S (1 << 7)
#define VIDEO_TGS_MODE_80L (0b11 << 6)
#define VIDEO_PAT_MODE_80L 0
#define VIDEO_TGS_MODE_80S (0b10 << 6)
#define VIDEO_PAT_MODE_80S 0
#elif defined(VIDEO_TS9347)
#define VIDEO_TGS_MODE_40L (0b00 << 6)
#define VIDEO_PAT_MODE_40L 0
#define VIDEO_TGS_MODE_40S (0b01 << 6)
#define VIDEO_PAT_MODE_40S 0
#define VIDEO_TGS_MODE_80L (0b11 << 6)
#define VIDEO_PAT_MODE_80L 0
#define VIDEO_TGS_MODE_80S (0b10 << 6)
#define VIDEO_PAT_MODE_80S 0
#endif

// Remaining bits in the TGS register.
#define VIDEO_TGS_INTERLACED (1 << 1)
#if defined(VIDEO_EF9345)
#define VIDEO_TGS_525LINES_NP (1 << 0)
#define VIDEO_TGS_HRESYNC_NP (1 << 2)
#define VIDEO_TGS_VRESYNC_NP (1 << 3)
#define VIDEO_TGS_CSYNC_OUT_NP (1 << 4)
#define VIDEO_TGS_SERVICE_ROW_Y1_NP (1 << 5)
#elif defined(VIDEO_TS9347)
#define VIDEO_TGS_SERVICE_ROW_LOW_NP (1 << 0)
#define VIDEO_TGS_SERVICE_SYNC_INPUT_NP (0b00 << 2)
#define VIDEO_TGS_SERVICE_SYNC_VSYNC_GEN_NP (0b01 << 2)
#define VIDEO_TGS_SERVICE_SYNC_VSYNC_IN_NP (0b10 << 2)
#define VIDEO_TGS_SERVICE_SYNC_CSYNC_IN_NP (0b11 << 2)
#define VIDEO_TGS_SERVICE_OUTPUT_BGR_NP (0b00 << 4)
#define VIDEO_TGS_SERVICE_OUTPUT_BIR_NP (0b01 << 4)
#define VIDEO_TGS_SERVICE_OUTPUT_P2P1HVS_NP (0b10 << 4)
#define VIDEO_TGS_SERVICE_OUTPUT_P2IHVS_NP (0b11 << 4)
#endif

// Remaining bits in the PAT register.
#define VIDEO_PAT_SERVICE_ROW_EN (1 << 0)
#define VIDEO_PAT_CONCEAL_EN (1 << 3)
#define VIDEO_PAT_INSERT_INLAY (0b00 << 4)
#define VIDEO_PAT_INSERT_BOXING_AND_INLAY (0b01 << 4)
#define VIDEO_PAT_INSERT_CHARACTER_MARK (0b10 << 4)
#define VIDEO_PAT_INSERT_ACTIVE_AREA_MARK (0b11 << 4)
#define VIDEO_PAT_FLASH_EN (1 << 6)
#if defined(VIDEO_EF9345)
#define VIDEO_PAT_UPPER_BULK_EN_NP (1 << 1)
#define VIDEO_PAT_LOWER_BULK_EN_NP (1 << 2)
// Portable alias for enabling both the upper and lower bulk.
#define VIDEO_PAT_BULK_EN \
  (VIDEO_PAT_UPPER_BULK_EN_NP | VIDEO_PAT_LOWER_BULK_EN_NP)
#elif defined(VIDEO_TS9347)
#define VIDEO_PAT_BULK_EN (1 << 1)  // No distinction between upper and lower.
#define VIDEO_PAT_PORT1_NP (1 << 2)
#define VIDEO_PAT_PORT2_NP (1 << 7)
#endif

// MAT register.
#define VIDEO_MAT_MARGIN_COLOR(n) (n)  // n=0-7
#define VIDEO_MAT_MARGIN_INSERT (1 << 3)
#define VIDEO_MAT_CURSOR_FIXED_COMPLEMENTED (0b00 << 4)
#define VIDEO_MAT_CURSOR_FLASH_COMPLEMENTED (0b10 << 4)
#define VIDEO_MAT_CURSOR_FIXED_UNDERLINED (0b01 << 4)
#define VIDEO_MAT_CURSOR_FLASH_UNDERLINED (0b11 << 4)
#define VIDEO_MAT_CURSOR_EN (1 << 6)
#define VIDEO_MAT_DOUBLE_HEIGHT (1 << 7)

#endif
