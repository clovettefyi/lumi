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

    OP_MOV = 0x10,

    OP_LOAD_B  = 0x11,
    OP_LOAD_W  = 0x12,
    OP_LOAD_DW = 0x13,
    OP_LOAD_QW = 0x14,

    OP_ADD     = 0x20,
    OP_ADD_B  = 0x21,
    OP_ADD_W  = 0x22,
    OP_ADD_DW = 0x23,
    OP_ADD_QW = 0x24,

    OP_SUB     = 0x28,
    OP_SUB_B  = 0x29,
    OP_SUB_W  = 0x2A,
    OP_SUB_DW = 0x2B,
    OP_SUB_QW = 0x2C,

    OP_MUL     = 0x30,
    OP_MUL_B  = 0x31,
    OP_MUL_W  = 0x32,
    OP_MUL_DW = 0x33,
    OP_MUL_QW = 0x34,

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
int32_t lumiRunVM(LumiVM* vm, const uint8_t* program, uint64_t program_size);

#endif
