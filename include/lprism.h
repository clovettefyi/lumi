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

#define CTC constexpr static

typedef struct {
    uint64_t registers[256];
    uint64_t pc;
} PrismCFrame;

typedef struct {
    uint64_t* gstack;

    struct {
        PrismCFrame* cframes;
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
} PrismVM;

typedef enum : uint8_t {
    PRISM_OP_NOP = 0x00,

    PRISM_OP_HALT  = 0x01,
    PRISM_OP_HALTR = 0x02,
    PRISM_OP_HALTI = 0x03,

    PRISM_OP_CALL  = 0x08,
    PRISM_OP_CALLR = 0x09,
    PRISM_OP_RET  = 0x0A,

    PRISM_OP_MOV  = 0x10,
    PRISM_OP_LOAD = 0x11,

    PRISM_OP_ADD  = 0x20,
    PRISM_OP_ADDI = 0x21,

    PRISM_OP_SUB  = 0x28,
    PRISM_OP_SUBI = 0x29,

    PRISM_OP_MUL  = 0x30,
    PRISM_OP_MULI = 0x31,

    PRISM_OP_JMP = 0x38,

    PRISM_OP_BEQ  = 0x39,
    PRISM_OP_BEQI = 0x3A,

    PRISM_OP_BNE  = 0x3B,
    PRISM_OP_BNEI = 0x3C,

    PRISM_OP_BGT  = 0x3D,
    PRISM_OP_BGTI = 0x3E,

    PRISM_OP_BLT  = 0x3F,
    PRISM_OP_BLTI = 0x40,

    PRISM_OP_BGE  = 0x41,
    PRISM_OP_BGEI = 0x42,

    PRISM_OP_BLE  = 0x43,
    PRISM_OP_BLEI = 0x44,

    PRISM_OP_ASP = 0x48,
} PrismOpCode;

typedef enum : uint8_t {
    PRISM_EX_OKAY    = 0x00,
    PRISM_EX_SIG_ERR = 0x80,
} PrsimExCode;

typedef enum : uint8_t {
    PRISM_SIG_VM_ERR,
    PRISM_SIG_PROG_ERR,
    PRISM_SIG_ILL,
    PRISM_SIG_SEGV_PC,
    PRISM_SIG_SEGV_SOF,
} PrismSignals;

/*
 -- INSTRUCTION LAYOUT --
*/

CTC uint64_t PRISM_INS_NOP_WIDTH = 1;

CTC uint64_t PRISM_INS_HALT_WIDTH = 1;

CTC uint64_t PRISM_INS_HALTR_WIDTH = 1 + 1;
CTC uint64_t PRISM_INS_HALTR_REG_OFFSET = 1;

CTC uint64_t PRISM_INS_HALTI_WIDTH = 1 + 8;
CTC uint64_t PRISM_INS_HALTI_IMM_OFFSET = 1;

CTC uint64_t PRISM_INS_CALL_WIDTH = 1 + 1 + 1 + 8;
CTC uint64_t PRISM_INS_CALL_SREG_OFFSET = 1;
CTC uint64_t PRISM_INS_CALL_EREG_OFFSET = 2;
CTC uint64_t PRISM_INS_CALL_PC_OFFSET = 3;

CTC uint64_t PRISM_INS_CALLR_WIDTH = 1 + 1 + 1 + 1;
CTC uint64_t PRISM_INS_CALLR_SREG_OFFSET = 1;
CTC uint64_t PRISM_INS_CALLR_EREG_OFFSET = 2;
CTC uint64_t PRISM_INS_CALLR_REG_OFFSET = 3;

CTC uint64_t PRISM_INS_RET_WIDTH = 1 + 1;
CTC uint64_t PRISM_INS_RET_REG_OFFSET = 1;

CTC uint64_t PRISM_INS_MOV_WIDTH = 1 + 1 + 1;
CTC uint64_t PRISM_INS_MOV_DST_OFFSET = 1;
CTC uint64_t PRISM_INS_MOV_SRC_OFFSET = 2;

CTC uint64_t PRISM_INS_LOAD_WIDTH = 1 + 1 + 8;
CTC uint64_t PRISM_INS_LOAD_DST_OFFSET = 1;
CTC uint64_t PRISM_INS_LOAD_IMM_OFFSET = 2;

#define PRISM_INS_ARITH_REG_LAYOUT(name) \
    CTC uint64_t PRISM_INS_##name##_WIDTH = 1 + 1 + 1 +1; \
    CTC uint64_t PRISM_INS_##name##_DST_OFFSET = 1; \
    CTC uint64_t PRISM_INS_##name##_SRC1_OFFSET = 2; \
    CTC uint64_t PRISM_INS_##name##_SRC2_OFFSET = 3;

#define PRISM_INS_ARITH_IMM_LAYOUT(name) \
    CTC uint64_t PRISM_INS_##name##_WIDTH = 1 + 1 + 1 + 8; \
    CTC uint64_t PRISM_INS_##name##_DST_OFFSET = 1; \
    CTC uint64_t PRISM_INS_##name##_SRC_OFFSET = 2; \
    CTC uint64_t PRISM_INS_##name##_IMM_OFFSET = 3;

PRISM_INS_ARITH_REG_LAYOUT(ADD);
PRISM_INS_ARITH_IMM_LAYOUT(ADDI);

PRISM_INS_ARITH_REG_LAYOUT(SUB);
PRISM_INS_ARITH_IMM_LAYOUT(SUBI);

PRISM_INS_ARITH_REG_LAYOUT(MUL);
PRISM_INS_ARITH_IMM_LAYOUT(MULI);

CTC uint64_t PRISM_INS_JMP_WIDTH = 1 + 8;
CTC uint64_t PRISM_INS_JMP_PC_OFFSET = 1;

#define PRISM_INS_BRANCH_REG_LAYOUT(name) \
    CTC uint64_t PRISM_INS_##name##_WIDTH = 1 + 1 + 1 + 8; \
    CTC uint64_t PRISM_INS_##name##_SRC1_OFFSET = 1; \
    CTC uint64_t PRISM_INS_##name##_SRC2_OFFSET = 2; \
    CTC uint64_t PRISM_INS_##name##_OFFSET_OFFSET = 3;

#define PRISM_INS_BRANCH_IMM_LAYOUT(name) \
    CTC uint64_t PRISM_INS_##name##_WIDTH = 1 + 1 + 8 + 8; \
    CTC uint64_t PRISM_INS_##name##_SRC_OFFSET = 1; \
    CTC uint64_t PRISM_INS_##name##_IMM_OFFSET = 2; \
    CTC uint64_t PRISM_INS_##name##_OFFSET_OFFSET = 10;

PRISM_INS_BRANCH_REG_LAYOUT(BEQ);
PRISM_INS_BRANCH_IMM_LAYOUT(BEQI);

PRISM_INS_BRANCH_REG_LAYOUT(BNE);
PRISM_INS_BRANCH_IMM_LAYOUT(BNEI);

PRISM_INS_BRANCH_REG_LAYOUT(BGT);
PRISM_INS_BRANCH_IMM_LAYOUT(BGTI);

PRISM_INS_BRANCH_REG_LAYOUT(BLT);
PRISM_INS_BRANCH_IMM_LAYOUT(BLTI);

PRISM_INS_BRANCH_REG_LAYOUT(BGE);
PRISM_INS_BRANCH_IMM_LAYOUT(BGEI);

PRISM_INS_BRANCH_REG_LAYOUT(BLE);
PRISM_INS_BRANCH_IMM_LAYOUT(BLEI);

CTC uint64_t PRISM_INS_ASP_WIDTH = 1 + 8;
CTC uint64_t PRISM_INS_ASP_IMM_OFFSET = 1;

PrismVM* prismCreate(uint8_t* program, uint64_t program_size);
void prismDestroy(PrismVM* vm);
uint8_t prismStepForward(PrismVM* vm, uint64_t step);

#endif
