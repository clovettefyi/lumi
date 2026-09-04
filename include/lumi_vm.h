/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org.
 */

#ifndef LUMI_VM_H
#define LUMI_VM_H

#include <stdint.h>

typedef struct LumiVM LumiVM;

typedef enum : uint8_t {
    OP_NOP   = 0x00,

    OP_CALL  = 0x01,
    OP_RET   = 0x02,

    OP_LOAD  = 0x0A,
    OP_LOADI = 0x0B,

    OP_ADD   = 0x10,
    OP_ADDI  = 0x11,
} LumiVM_OpCode;

typedef enum : int32_t {
    EX_OKAY        = 0x00000000,

    EX_PROGRAM_ERR = 0x7FFFFFFE,
    EX_STATE_ERR   = 0x7FFFFFFF,
} LumiVM_ExCode;

typedef enum : uint8_t {
    IMMT_BYTE  = 0b00000000,
    IMMT_WORD  = 0b00000001,
    IMMT_DWORD = 0b00000010,
    IMMT_QWORD = 0b00000100,
} LumiVM_ImmType;

LumiVM* lumiCreateVM(void);
void lumiDestroyVM(LumiVM* vm);
int32_t lumiRunVM(LumiVM* vm, const uint8_t* program);

#endif
