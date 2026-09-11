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

typedef struct {
    uint64_t registers[256];
    uint64_t pc;
} PrismCFrame;

typedef struct {
    struct {
        PrismCFrame* cframes;
        uint64_t fp;
    } cstack;

    struct {
        uint8_t* data;
        uint64_t sp;
        uint64_t bp;
    } dstack;

    uint64_t pc;
} PrismVM;

typedef enum : uint8_t {
    PRISM_OP_NOP  = 0x00,
    PRISM_OP_HALT = 0X01,

    PRISM_OP_CALL  = 0x08,
    PRISM_OP_CALLR = 0x09,
    PRISM_OP_RET   = 0x0A,

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

PrismVM* prismCreateVM(void);
void prismDestroyVM(PrismVM* vm);
uint8_t prismRunVM(PrismVM* vm, const uint8_t* program, uint64_t program_size);

#endif
