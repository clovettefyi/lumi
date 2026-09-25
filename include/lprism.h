/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org.
 */

#ifndef LPRISM_H
#define LPRISM_H

#include <stddef.h>
#include <stdint.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
    #define LP_TYPED_ENUM(type) : type
#else
    #define LP_TYPED_ENUM(type)
#endif

#define LP_REG_COUNT 256

typedef struct {
    uint64_t registers[LP_REG_COUNT];

    uint8_t* stack;
    uint64_t stack_size;

    uint8_t* program;
    uint64_t program_size;
    uint64_t pc;
} LpInstance;

#define LP_BP_REG 0xFF
#define LP_SP_REG 0xFE

typedef enum LP_TYPED_ENUM(uint8_t) {
    LP_INS_EX = 0xFF,

    LP_INS_NOP = 0x01,

    LP_INS_JAL  = 0x10,
    LP_INS_JALR = 0x11,
    LP_INS_RET  = 0x12,

    LP_INS_MOV  = 0x20,
    LP_INS_MOVI = 0x21,

    LP_INS_ADD  = 0x30,
    LP_INS_ADDI = 0x31,

    LP_INS_SUB  = 0x32,
    LP_INS_SUBI = 0x33,

    LP_INS_MUL  = 0x34,
    LP_INS_MULI = 0x35,

    LP_INS_JMP = 0x36,

    LP_INS_BEQ  = 0x40,
    LP_INS_BEQI = 0x41,

    LP_INS_BNE  = 0x42,
    LP_INS_BNEI = 0x43,

    LP_INS_BGT  = 0x44,
    LP_INS_BGTI = 0x45,

    LP_INS_BLT  = 0x46,
    LP_INS_BLTI = 0x47,

    LP_INS_BGE  = 0x48,
    LP_INS_BGEI = 0x49,

    LP_INS_BLE  = 0x4A,
    LP_INS_BLEI = 0x4B,

    LP_INS_LD_B = 0x50,
    LP_INS_LD_W = 0x51,
    LP_INS_LD_D = 0x52,
    LP_INS_LD_Q = 0x53,

    LP_INS_LDO_B = 0x54,
    LP_INS_LDO_W = 0x55,
    LP_INS_LDO_D = 0x56,
    LP_INS_LDO_Q = 0x57,

    LP_INS_LDR_B = 0x58,
    LP_INS_LDR_W = 0x59,
    LP_INS_LDR_D = 0x5A,
    LP_INS_LDR_Q = 0x5B,

    LP_INS_ST_B = 0x60,
    LP_INS_ST_W = 0x61,
    LP_INS_ST_D = 0x62,
    LP_INS_ST_Q = 0x63,

    LP_INS_STO_B = 0x64,
    LP_INS_STO_W = 0x65,
    LP_INS_STO_D = 0x66,
    LP_INS_STO_Q = 0x67,

    LP_INS_STR_B = 0x68,
    LP_INS_STR_W = 0x69,
    LP_INS_STR_D = 0x6A,
    LP_INS_STR_Q = 0x6B,

    LP_INS_EXT = 0xEE,
} LpInstruction;

typedef enum LP_TYPED_ENUM(uint8_t) {
    LP_EX_OKAY    = 0x00,
    LP_EX_SIG_ERR = 0x80,
} LpExitCode;

typedef enum LP_TYPED_ENUM(uint8_t) {
    LP_SIG_VM_ERR,
    LP_SIG_PROG_ERR,
    LP_SIG_ILL,
    LP_SIG_SEGV_PC,
    LP_SIG_SEGV_SOF,
    LP_SIG_SEGV_SUF,
} LpSignal;

/*
 -- INSTRUCTION LAYOUT --
*/

#define LP_INS_NOP_WIDTH 1

#define LP_INS_EX_WIDTH 1

#define LP_INS_JAL_WIDTH 1 + 8
#define LP_INS_JAL_ADDR_OFFSET 1

#define LP_INS_JALR_WIDTH 1 + 1
#define LP_INS_JALR_DST_OFFSET 1

#define LP_INS_RET_WIDTH 1

#define LP_INS_MOV_WIDTH 1 + 1 + 1
#define LP_INS_MOV_DST_OFFSET 1
#define LP_INS_MOV_SRC_OFFSET 2

#define LP_INS_MOVI_WIDTH 1 + 1 + 8
#define LP_INS_MOVI_DST_OFFSET 1
#define LP_INS_MOVI_IMM_OFFSET 2

#define LP_INS_ADD_WIDTH 1 + 1 + 1 + 1
#define LP_INS_ADD_DST_OFFSET 1
#define LP_INS_ADD_SRC1_OFFSET 2
#define LP_INS_ADD_SRC2_OFFSET 3

#define LP_INS_SUB_WIDTH 1 + 1 + 1 + 1
#define LP_INS_SUB_DST_OFFSET 1
#define LP_INS_SUB_SRC1_OFFSET 2
#define LP_INS_SUB_SRC2_OFFSET 3

#define LP_INS_MUL_WIDTH 1 + 1 + 1 + 1
#define LP_INS_MUL_DST_OFFSET 1
#define LP_INS_MUL_SRC1_OFFSET 2
#define LP_INS_MUL_SRC2_OFFSET 3

#define LP_INS_ADDI_WIDTH 1 + 1 + 1 + 8
#define LP_INS_ADDI_DST_OFFSET 1
#define LP_INS_ADDI_SRC_OFFSET 2
#define LP_INS_ADDI_IMM_OFFSET 3

#define LP_INS_SUBI_WIDTH 1 + 1 + 1 + 8
#define LP_INS_SUBI_DST_OFFSET 1
#define LP_INS_SUBI_SRC_OFFSET 2
#define LP_INS_SUBI_IMM_OFFSET 3

#define LP_INS_MULI_WIDTH 1 + 1 + 1 + 8
#define LP_INS_MULI_DST_OFFSET 1
#define LP_INS_MULI_SRC_OFFSET 2
#define LP_INS_MULI_IMM_OFFSET 3

#define LP_INS_JMP_WIDTH 1 + 8
#define LP_INS_JMP_ADDR_OFFSET 1

#define LP_INS_BEQ_WIDTH 1 + 1 + 1 + 8
#define LP_INS_BEQ_SRC1_OFFSET 1
#define LP_INS_BEQ_SRC2_OFFSET 2
#define LP_INS_BEQ_OFFSET_OFFSET 3

#define LP_INS_BNE_WIDTH 1 + 1 + 1 + 8
#define LP_INS_BNE_SRC1_OFFSET 1
#define LP_INS_BNE_SRC2_OFFSET 2
#define LP_INS_BNE_OFFSET_OFFSET 3

#define LP_INS_BGT_WIDTH 1 + 1 + 1 + 8
#define LP_INS_BGT_SRC1_OFFSET 1
#define LP_INS_BGT_SRC2_OFFSET 2
#define LP_INS_BGT_OFFSET_OFFSET 3

#define LP_INS_BLT_WIDTH 1 + 1 + 1 + 8
#define LP_INS_BLT_SRC1_OFFSET 1
#define LP_INS_BLT_SRC2_OFFSET 2
#define LP_INS_BLT_OFFSET_OFFSET 3

#define LP_INS_BGE_WIDTH 1 + 1 + 1 + 8
#define LP_INS_BGE_SRC1_OFFSET 1
#define LP_INS_BGE_SRC2_OFFSET 2
#define LP_INS_BGE_OFFSET_OFFSET 3

#define LP_INS_BLE_WIDTH 1 + 1 + 1 + 8
#define LP_INS_BLE_SRC1_OFFSET 1
#define LP_INS_BLE_SRC2_OFFSET 2
#define LP_INS_BLE_OFFSET_OFFSET 3

#define LP_INS_BEQI_WIDTH 1 + 1 + 8 + 8
#define LP_INS_BEQI_SRC_OFFSET 1
#define LP_INS_BEQI_IMM_OFFSET 2
#define LP_INS_BEQI_OFFSET_OFFSET 10

#define LP_INS_BNEI_WIDTH 1 + 1 + 8 + 8
#define LP_INS_BNEI_SRC_OFFSET 1
#define LP_INS_BNEI_IMM_OFFSET 2
#define LP_INS_BNEI_OFFSET_OFFSET 10

#define LP_INS_BGTI_WIDTH 1 + 1 + 8 + 8
#define LP_INS_BGTI_SRC_OFFSET 1
#define LP_INS_BGTI_IMM_OFFSET 2
#define LP_INS_BGTI_OFFSET_OFFSET 10

#define LP_INS_BLTI_WIDTH 1 + 1 + 8 + 8
#define LP_INS_BLTI_SRC_OFFSET 1
#define LP_INS_BLTI_IMM_OFFSET 2
#define LP_INS_BLTI_OFFSET_OFFSET 10

#define LP_INS_BGEI_WIDTH 1 + 1 + 8 + 8
#define LP_INS_BGEI_SRC_OFFSET 1
#define LP_INS_BGEI_IMM_OFFSET 2
#define LP_INS_BGEI_OFFSET_OFFSET 10

#define LP_INS_BLEI_WIDTH 1 + 1 + 8 + 8
#define LP_INS_BLEI_SRC_OFFSET 1
#define LP_INS_BLEI_IMM_OFFSET 2
#define LP_INS_BLEI_OFFSET_OFFSET 10

#define LP_INS_LD_B_WIDTH 1 + 1 + 8
#define LP_INS_LD_B_SRC_OFFSET 1
#define LP_INS_LD_B_ADDR_OFFSET 2

#define LP_INS_LD_W_WIDTH 1 + 1 + 8
#define LP_INS_LD_W_SRC_OFFSET 1
#define LP_INS_LD_W_ADDR_OFFSET 2

#define LP_INS_LD_D_WIDTH 1 + 1 + 8
#define LP_INS_LD_D_SRC_OFFSET 1
#define LP_INS_LD_D_ADDR_OFFSET 2

#define LP_INS_LD_Q_WIDTH 1 + 1 + 8
#define LP_INS_LD_Q_SRC_OFFSET 1
#define LP_INS_LD_Q_ADDR_OFFSET 2

#define LP_INS_LDO_B_WIDTH 1 + 1 + 1 + 8
#define LP_INS_LDO_B_SRC_OFFSET 1
#define LP_INS_LDO_B_DST_OFFSET 2
#define LP_INS_LDO_B_OFFSET_OFFSET 3

#define LP_INS_LDO_W_WIDTH 1 + 1 + 1 + 8
#define LP_INS_LDO_W_SRC_OFFSET 1
#define LP_INS_LDO_W_DST_OFFSET 2
#define LP_INS_LDO_W_OFFSET_OFFSET 3

#define LP_INS_LDO_D_WIDTH 1 + 1 + 1 + 8
#define LP_INS_LDO_D_SRC_OFFSET 1
#define LP_INS_LDO_D_DST_OFFSET 2
#define LP_INS_LDO_D_OFFSET_OFFSET 3

#define LP_INS_LDO_Q_WIDTH 1 + 1 + 1 + 8
#define LP_INS_LDO_Q_SRC_OFFSET 1
#define LP_INS_LDO_Q_DST_OFFSET 2
#define LP_INS_LDO_Q_OFFSET_OFFSET 3

#define LP_INS_LDR_B_WIDTH 1 + 1 + 1 + 1
#define LP_INS_LDR_B_SRC_OFFSET 1
#define LP_INS_LDR_B_DST_OFFSET 2
#define LP_INS_LDR_B_OFFSET_REG_OFFSET 3

#define LP_INS_LDR_W_WIDTH 1 + 1 + 1 + 1
#define LP_INS_LDR_W_SRC_OFFSET 1
#define LP_INS_LDR_W_DST_OFFSET 2
#define LP_INS_LDR_W_OFFSET_REG_OFFSET 3

#define LP_INS_LDR_D_WIDTH 1 + 1 + 1 + 1
#define LP_INS_LDR_D_SRC_OFFSET 1
#define LP_INS_LDR_D_DST_OFFSET 2
#define LP_INS_LDR_D_OFFSET_REG_OFFSET 3

#define LP_INS_LDR_Q_WIDTH 1 + 1 + 1 + 1
#define LP_INS_LDR_Q_SRC_OFFSET 1
#define LP_INS_LDR_Q_DST_OFFSET 2
#define LP_INS_LDR_Q_OFFSET_REG_OFFSET 3

#define LP_INS_ST_B_WIDTH 1 + 1 + 8
#define LP_INS_ST_B_DST_OFFSET 1
#define LP_INS_ST_B_ADDR_OFFSET 2

#define LP_INS_ST_W_WIDTH 1 + 1 + 8
#define LP_INS_ST_W_DST_OFFSET 1
#define LP_INS_ST_W_ADDR_OFFSET 2

#define LP_INS_ST_D_WIDTH 1 + 1 + 8
#define LP_INS_ST_D_DST_OFFSET 1
#define LP_INS_ST_D_ADDR_OFFSET 2

#define LP_INS_ST_Q_WIDTH 1 + 1 + 8
#define LP_INS_ST_Q_DST_OFFSET 1
#define LP_INS_ST_Q_ADDR_OFFSET 2

#define LP_INS_STO_B_WIDTH 1 + 1 + 1 + 8
#define LP_INS_STO_B_DST_OFFSET 1
#define LP_INS_STO_B_SRC_OFFSET 2
#define LP_INS_STO_B_OFFSET_OFFSET 3

#define LP_INS_STO_W_WIDTH 1 + 1 + 1 + 8
#define LP_INS_STO_W_DST_OFFSET 1
#define LP_INS_STO_W_SRC_OFFSET 2
#define LP_INS_STO_W_OFFSET_OFFSET 3

#define LP_INS_STO_D_WIDTH 1 + 1 + 1 + 8
#define LP_INS_STO_D_DST_OFFSET 1
#define LP_INS_STO_D_SRC_OFFSET 2
#define LP_INS_STO_D_OFFSET_OFFSET 3

#define LP_INS_STO_Q_WIDTH 1 + 1 + 1 + 8
#define LP_INS_STO_Q_DST_OFFSET 1
#define LP_INS_STO_Q_SRC_OFFSET 2
#define LP_INS_STO_Q_OFFSET_OFFSET 3

#define LP_INS_STR_B_WIDTH 1 + 1 + 1 + 1
#define LP_INS_STR_B_DST_OFFSET 1
#define LP_INS_STR_B_SRC_OFFSET 2
#define LP_INS_STR_B_OFFSET_REG_OFFSET 3

#define LP_INS_STR_W_WIDTH 1 + 1 + 1 + 1
#define LP_INS_STR_W_DST_OFFSET 1
#define LP_INS_STR_W_SRC_OFFSET 2
#define LP_INS_STR_W_OFFSET_REG_OFFSET 3

#define LP_INS_STR_D_WIDTH 1 + 1 + 1 + 1
#define LP_INS_STR_D_DST_OFFSET 1
#define LP_INS_STR_D_SRC_OFFSET 2
#define LP_INS_STR_D_OFFSET_REG_OFFSET 3

#define LP_INS_STR_Q_WIDTH 1 + 1 + 1 + 1
#define LP_INS_STR_Q_DST_OFFSET 1
#define LP_INS_STR_Q_SRC_OFFSET 2
#define LP_INS_STR_Q_OFFSET_REG_OFFSET 3

LpInstance* lpCreate(uint64_t stack_size, uint8_t* program, uint64_t program_size);
void lpDestroy(LpInstance* vm);

uint8_t lpRun(LpInstance* vm);

#endif
