/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org.
 */

#ifndef LUMI_VM_H
#define LUMI_VM_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t registers[256];
    uint64_t pc;
} LumiVMCFrame;

typedef struct {
    struct {
        LumiVMCFrame* cframes;
        uint64_t fp;
        uint64_t accumulator;
    } cstack;

    struct {
        uint8_t* data;
        uint64_t sp;
        uint64_t bp;
    } dstack;

    uint64_t pc;
} LumiVM;

typedef enum : uint8_t {
    OP_NOP  = 0x00,
    OP_HALT = 0X01,

    OP_CALL = 0x08,
    OP_RET  = 0x09,

    OP_LOAD     = 0x10,
    OP_LOADI_B  = 0x11,
    OP_LOADI_W  = 0x12,
    OP_LOADI_DW = 0x13,
    OP_LOADI_QW = 0x14,

    OP_STR = 0x18,

    OP_ADD     = 0x20,
    OP_ADDI_B  = 0x21,
    OP_ADDI_W  = 0x22,
    OP_ADDI_DW = 0x23,
    OP_ADDI_QW = 0x24,

    OP_SUB     = 0x28,
    OP_SUBI_B  = 0x29,
    OP_SUBI_W  = 0x2A,
    OP_SUBI_DW = 0x2B,
    OP_SUBI_QW = 0x2C,

    OP_MUL     = 0x30,
    OP_MULI_B  = 0x31,
    OP_MULI_W  = 0x32,
    OP_MULI_DW = 0x33,
    OP_MULI_QW = 0x34,

    OP_JMP = 0x38,
    OP_JNZ = 0x39,
} LumiVM_OpCode;

typedef enum : int32_t {
    EX_OKAY = 0x00000000,

    EX_ABRUPT_END = 0x70000001,
    EX_STACK_OF   = 0x70000002,
    EX_ILL_INS    = 0x70000003,
    EX_INV_INS    = 0x70000004,

    EX_PROGRAM_ERR = 0x7FFFFFFE,
    EX_STATE_ERR   = 0x7FFFFFFF,
} LumiVM_ExCode;

LumiVM* lumiCreateVM(void);
void lumiDestroyVM(LumiVM* vm);

uint64_t lumiVMGetAccumulator(LumiVM* vm);

int32_t lumiRunVM(LumiVM* vm, const uint8_t* program, uint64_t program_size);

#endif
