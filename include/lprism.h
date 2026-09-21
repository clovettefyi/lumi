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
    #define LP_CONST(type, name, val) constexpr static type name = val;
    #define LP_TYPED_ENUM(type) : type
#else
    #define LP_CONST(type, name, val) enum { name = val };
    #define LP_TYPED_ENUM(type)
#endif

typedef struct {
    uint64_t registers[256];
    uint64_t pc;
} LpCFrame;

typedef struct {
    uint64_t* gstack;

    struct {
        LpCFrame* cframes;
        uint64_t fp;
    } cstack;

    struct {
        uint64_t* data;
        uint64_t sp;
        uint64_t bp;
    } dstack;

    uint8_t* program;
    uint64_t program_size;
    uint64_t pc;
} LpInstance;

typedef enum LP_TYPED_ENUM(uint8_t) {
    LP_INS_EX = 0xFF,

    LP_INS_NOP = 0x01,

    LP_INS_JAL  = 0x10,
    LP_INS_JALR = 0x11,
    LP_INS_RET  = 0x12,

    LP_INS_MOV  = 0x20,
    LP_INS_LDI = 0x21,

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

    LP_INS_AS  = 0x50,
    LP_INS_ASR = 0x51,

    LP_INS_FS = 0x52,
    LP_INS_FSR = 0x53,

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

LP_CONST(uint64_t, LP_INS_NOP_WIDTH, 1);

LP_CONST(uint64_t, LP_INS_EX_WIDTH, 1);

LP_CONST(uint64_t, LP_INS_JAL_WIDTH, 1 + 1 + 1 + 8);
LP_CONST(uint64_t, LP_INS_JAL_SREG_OFFSET, 1);
LP_CONST(uint64_t, LP_INS_JAL_EREG_OFFSET, 2);
LP_CONST(uint64_t, LP_INS_JAL_PC_OFFSET, 3);

LP_CONST(uint64_t, LP_INS_JALR_WIDTH, 1 + 1 + 1 + 1);
LP_CONST(uint64_t, LP_INS_JALR_SREG_OFFSET, 1);
LP_CONST(uint64_t, LP_INS_JALR_EREG_OFFSET, 2);
LP_CONST(uint64_t, LP_INS_JALR_REG_OFFSET, 3);

LP_CONST(uint64_t, LP_INS_RET_WIDTH, 1 + 1);
LP_CONST(uint64_t, LP_INS_RET_REG_OFFSET, 1);

LP_CONST(uint64_t, LP_INS_MOV_WIDTH, 1 + 1 + 1);
LP_CONST(uint64_t, LP_INS_MOV_DST_OFFSET, 1);
LP_CONST(uint64_t, LP_INS_MOV_SRC_OFFSET, 2);

LP_CONST(uint64_t, LP_INS_LDI_WIDTH, 1 + 1 + 8);
LP_CONST(uint64_t, LP_INS_LDI_DST_OFFSET, 1);
LP_CONST(uint64_t, LP_INS_LDI_IMM_OFFSET, 2);

#define LP_INS_ARITH_REG_LAYOUT(name) \
    LP_CONST(uint64_t, LP_INS_##name##_WIDTH, 1 + 1 + 1 +1); \
    LP_CONST(uint64_t, LP_INS_##name##_DST_OFFSET, 1); \
    LP_CONST(uint64_t, LP_INS_##name##_SRC1_OFFSET, 2); \
    LP_CONST(uint64_t, LP_INS_##name##_SRC2_OFFSET, 3);

#define LP_INS_ARITH_IMM_LAYOUT(name) \
    LP_CONST(uint64_t, LP_INS_##name##_WIDTH, 1 + 1 + 1 + 8); \
    LP_CONST(uint64_t, LP_INS_##name##_DST_OFFSET, 1); \
    LP_CONST(uint64_t, LP_INS_##name##_SRC_OFFSET, 2); \
    LP_CONST(uint64_t, LP_INS_##name##_IMM_OFFSET, 3);

LP_INS_ARITH_REG_LAYOUT(ADD);
LP_INS_ARITH_IMM_LAYOUT(ADDI);

LP_INS_ARITH_REG_LAYOUT(SUB);
LP_INS_ARITH_IMM_LAYOUT(SUBI);

LP_INS_ARITH_REG_LAYOUT(MUL);
LP_INS_ARITH_IMM_LAYOUT(MULI);

LP_CONST(uint64_t, LP_INS_JMP_WIDTH, 1 + 8);
LP_CONST(uint64_t, LP_INS_JMP_PC_OFFSET, 1);

#define LP_INS_BRANCH_REG_LAYOUT(name) \
    LP_CONST(uint64_t, LP_INS_##name##_WIDTH, 1 + 1 + 1 + 8); \
    LP_CONST(uint64_t, LP_INS_##name##_SRC1_OFFSET, 1); \
    LP_CONST(uint64_t, LP_INS_##name##_SRC2_OFFSET, 2); \
    LP_CONST(uint64_t, LP_INS_##name##_OFFSET_OFFSET, 3);

#define LP_INS_BRANCH_IMM_LAYOUT(name) \
    LP_CONST(uint64_t, LP_INS_##name##_WIDTH, 1 + 1 + 8 + 8); \
    LP_CONST(uint64_t, LP_INS_##name##_SRC_OFFSET, 1); \
    LP_CONST(uint64_t, LP_INS_##name##_IMM_OFFSET, 2); \
    LP_CONST(uint64_t, LP_INS_##name##_OFFSET_OFFSET, 10);

LP_INS_BRANCH_REG_LAYOUT(BEQ);
LP_INS_BRANCH_IMM_LAYOUT(BEQI);

LP_INS_BRANCH_REG_LAYOUT(BNE);
LP_INS_BRANCH_IMM_LAYOUT(BNEI);

LP_INS_BRANCH_REG_LAYOUT(BGT);
LP_INS_BRANCH_IMM_LAYOUT(BGTI);

LP_INS_BRANCH_REG_LAYOUT(BLT);
LP_INS_BRANCH_IMM_LAYOUT(BLTI);

LP_INS_BRANCH_REG_LAYOUT(BGE);
LP_INS_BRANCH_IMM_LAYOUT(BGEI);

LP_INS_BRANCH_REG_LAYOUT(BLE);
LP_INS_BRANCH_IMM_LAYOUT(BLEI);

LP_CONST(uint64_t, LP_INS_AS_WIDTH, 1 + 8);
LP_CONST(uint64_t, LP_INS_AS_IMM_OFFSET, 1);

LP_CONST(uint64_t, LP_INS_ASR_WIDTH, 1 + 1);
LP_CONST(uint64_t, LP_INS_ASR_REG_OFFSET, 1);

LP_CONST(uint64_t, LP_INS_FS_WIDTH, 1 + 8);
LP_CONST(uint64_t, LP_INS_FS_IMM_OFFSET, 1);

LP_CONST(uint64_t, LP_INS_FSR_WIDTH, 1 + 1);
LP_CONST(uint64_t, LP_INS_FSR_REG_OFFSET, 1);

LpInstance* lpCreate(uint8_t* program, uint64_t program_size);
void lpDestroy(LpInstance* vm);

uint8_t lpRun(LpInstance* vm);

#endif
