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

    OP_MOV  = 0x10,
    OP_LOAD = 0x11,

    OP_ADD  = 0x20,
    OP_ADDI = 0x21,

    OP_SUB  = 0x28,
    OP_SUBI = 0x29,

    OP_MUL  = 0x30,
    OP_MULI = 0x31,

    OP_JMP = 0x38,
    OP_JNZ = 0x39,
} LumiVM_OpCode;

typedef enum : uint8_t {
    EX_OKAY    = 0x00,
    EX_SIG_ERR = 0x80,
} LumiVM_ExCode;

typedef enum : uint8_t {
    SIG_VM_ERR,
    SIG_PROG_ERR,
    SIG_ILL,
    SIG_SEGV_PC,
    SIG_SEGV_SOF,
} LumiVM_Signals;

LumiVM* lumiCreateVM(void);
void lumiDestroyVM(LumiVM* vm);
uint8_t lumiRunVM(LumiVM* vm, const uint8_t* program, uint64_t program_size);

#endif
