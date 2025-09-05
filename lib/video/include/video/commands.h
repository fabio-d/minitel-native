#ifndef LIB_VIDEO_INCLUDE_VIDEO_COMMANDS_H
#define LIB_VIDEO_INCLUDE_VIDEO_COMMANDS_H

#include "board/definitions.h"

// "Portable" commands are commands that work the same way on both chips. They
// are safe to use in programs targeting both.
//
// "Non portable" commands (ending with "_NP") do not exist or are different in
// the other chip.

#if defined(VIDEO_EF9345)
#define VIDEO_CMD_IND_NP 0b10000000  // Indirect
#define VIDEO_CMD_KRF_NP 0b00000000  // 40 Characters - 24 bits
#define VIDEO_CMD_KRG_NP 0b00000010  // 40 Characters - 16 bits
#define VIDEO_CMD_KRC_NP 0b01000000  // 80 Characters - 8 bits
#define VIDEO_CMD_KRL_NP 0b01010000  // 80 Characters - 12 bits
#define VIDEO_CMD_KRV_NP 0b00100000  // 40 Characters Variable
#define VIDEO_CMD_EXP_NP 0b01100000  // Expansion
#define VIDEO_CMD_CMP_NP 0b01110000  // Compression
#define VIDEO_CMD_KRE_NP 0b00010000  // Expanded Characters
#define VIDEO_CMD_OCT_NP 0b00110000  // Byte
#define VIDEO_CMD_MVB_NP 0b11010000  // Move Buffer
#define VIDEO_CMD_MVD_NP 0b11100000  // Move Double Buffer
#define VIDEO_CMD_MVT_NP 0b11110000  // Move Triple Buffer
#define VIDEO_CMD_CLF_NP 0b00000101  // Clear Page - 24 bits
#define VIDEO_CMD_CLG_NP 0b00000111  // Clear Page - 16 bits
#define VIDEO_CMD_VSM_NP 0b10011001  // Vertical Sync Mask Set
#define VIDEO_CMD_VRM_NP 0b10010101  // Vertical Sync Mask Reset
#define VIDEO_CMD_INY_NP 0b10110000  // Increment Y
#define VIDEO_CMD_NOP_NP 0b10010001  // No Operation
#elif defined(VIDEO_TS9347)
#define VIDEO_CMD_IND_NP 0b10000000  // Indirect
#define VIDEO_CMD_TLM_NP 0b00000000  // 40 Characters - 24 bits
#define VIDEO_CMD_TLA_NP 0b00100010  // 40 Characters - 24 bits
#define VIDEO_CMD_TSM_NP 0b01100000  // 40 Characters - 16 bits
#define VIDEO_CMD_TSA_NP 0b01110000  // 40 Characters - 16 bits
#define VIDEO_CMD_KRS_NP 0b01000000  // 80 Characters - 8 bits
#define VIDEO_CMD_KRL_NP 0b01010000  // 80 Characters - 12 bits
#define VIDEO_CMD_TBM_NP 0b00110000  // Byte
#define VIDEO_CMD_TBA_NP 0b00110100  // Byte
#define VIDEO_CMD_MVB_NP 0b11010000  // Move Buffer
#define VIDEO_CMD_MVD_NP 0b11100000  // Move Double Buffer
#define VIDEO_CMD_MVT_NP 0b11110000  // Move Triple Buffer
#define VIDEO_CMD_CLF_NP 0b00000101  // Clear Page - 24 bits
#define VIDEO_CMD_CLG_NP 0b01100101  // Clear Page - 16 bits
#define VIDEO_CMD_VSM_NP 0b10011001  // Vertical Sync Mask Set
#define VIDEO_CMD_VRM_NP 0b10010101  // Vertical Sync Mask Reset
#define VIDEO_CMD_INY_NP 0b10110000  // Increment Y
#define VIDEO_CMD_NOP_NP 0b10010001  // No Operation
#endif

// Define aliases for portable commands. When the names are different, the
// TS9347 naming is preferred.
#define VIDEO_CMD_IND VIDEO_CMD_IND_NP
#if defined(VIDEO_EF9345)
#define VIDEO_CMD_TLM VIDEO_CMD_KRF_NP
#define VIDEO_CMD_TSM VIDEO_CMD_KRG_NP
#define VIDEO_CMD_KRS VIDEO_CMD_KRC_NP
#define VIDEO_CMD_KRL VIDEO_CMD_KRL_NP
#define VIDEO_CMD_TBM VIDEO_CMD_OCT_NP
#define VIDEO_CMD_TBA (VIDEO_CMD_OCT_NP | 0b0100)
#elif defined(VIDEO_TS9347)
#define VIDEO_CMD_TLM VIDEO_CMD_TLM_NP
#define VIDEO_CMD_TSM 0b00000010  // HACK for MAME, should be VIDEO_CMD_TSM_NP
#define VIDEO_CMD_KRS VIDEO_CMD_KRS_NP
#define VIDEO_CMD_KRL VIDEO_CMD_KRL_NP
#define VIDEO_CMD_TBM VIDEO_CMD_TBM_NP
#define VIDEO_CMD_TBA VIDEO_CMD_TBA_NP
#endif
#define VIDEO_CMD_MVB VIDEO_CMD_MVB_NP
#define VIDEO_CMD_MVD VIDEO_CMD_MVD_NP
#define VIDEO_CMD_MVT VIDEO_CMD_MVT_NP
#if defined(VIDEO_EF9345)
#define VIDEO_CMD_CLL VIDEO_CMD_CLF_NP
#define VIDEO_CMD_CLS VIDEO_CMD_CLG_NP
#elif defined(VIDEO_TS9347)
#define VIDEO_CMD_CLL VIDEO_CMD_CLL_NP
#define VIDEO_CMD_CLS VIDEO_CMD_CLS_NP
#endif
#define VIDEO_CMD_VSM VIDEO_CMD_VSM_NP
#define VIDEO_CMD_VRM VIDEO_CMD_VRM_NP
#define VIDEO_CMD_INY VIDEO_CMD_INY_NP
#define VIDEO_CMD_NOP VIDEO_CMD_NOP_NP

// Memory operation (for several commands).
#define VIDEO_MEM_READ 8
#define VIDEO_MEM_POSTINCR 1

// Register selector (for the IND command).
#define VIDEO_IND_ROM 0
#define VIDEO_IND_TGS 1
#define VIDEO_IND_MAT 2
#define VIDEO_IND_PAT 3
#define VIDEO_IND_DOR 4
#define VIDEO_IND_ROR 7

// Move parameters (for MVB, MVD and MVL).
#define VIDEO_MOVE_DIR_MP_TO_AP (0b01 << 2)
#define VIDEO_MOVE_DIR_AP_TO_MP (0b10 << 2)
#define VIDEO_MOVE_STOP_EOB 0b01  // Stop at end of buffer
#define VIDEO_MOVE_STOP_NO 0b10   // No stop

#endif
