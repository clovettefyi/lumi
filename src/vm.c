/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org.
 */

#include "lumi_vm.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

constexpr static size_t CFRAME_REGISTERS_COUNT = 256;
constexpr static size_t CFRAME_COUNT = 128;
constexpr static size_t DSTACK_BYTE_COUNT = 1024 * 1024;

constexpr static uint64_t INS_NOP_WIDTH = 1;

// constexpr static uint64_t INS_HALT_WIDTH = 1;

constexpr static uint64_t INS_CALL_WIDTH = 1 + 1 + 1 + 8;
constexpr static uint64_t INS_CALL_SREG_OFFSET = 1;
constexpr static uint64_t INS_CALL_EREG_OFFSET = 2;
constexpr static uint64_t INS_CALL_PC_OFFSET = 3;

// constexpr static uint64_t INS_RET_WIDTH = 1;

constexpr static uint64_t INS_LOAD_WIDTH = 1 + 1;
constexpr static uint64_t INS_LOAD_REG_OFFSET = 1;

constexpr static uint64_t INS_LOADI_WIDTH = 1 + 1;
constexpr static uint64_t INS_LOADI_FLAG_OFFSET = 1;
constexpr static uint64_t INS_LOADI_IMM_OFFSET = 2;

constexpr static uint64_t INS_ADD_WIDTH = 1 + 1;
constexpr static uint64_t INS_ADD_REG_OFFSET = 1;

constexpr static uint64_t INS_ADDI_WIDTH = 1 + 1;
constexpr static uint64_t INS_ADDI_FLAG_OFFSET = 1;
constexpr static uint64_t INS_ADDI_IMM_OFFSET = 2;

typedef struct {
    uint64_t registers[CFRAME_REGISTERS_COUNT];
    uint64_t fp;
    uint64_t pc;
} CFrame;

struct LumiVM {
    struct {
        CFrame* cframes;
        uint64_t fp;
        uint64_t accumulator;
    } cstack;

    struct {
        uint8_t* data;
        uint64_t sp;
        uint64_t bp;
    } dstack;

    uint64_t pc;
};

static inline bool hasNext(uint64_t pc, uint64_t program_size, uint64_t bytes) {
    if (pc + bytes > program_size) return false;
    return true;
}

static inline uint8_t getNext(uint64_t pc, const uint8_t* program) {
    return program[pc];
}

static inline uint16_t getNext2(uint64_t pc, const uint8_t* program) {
    uint8_t byte0 = program[pc];
    uint8_t byte1 = program[pc + 1];

    return ((uint16_t)byte1 << 8) | byte0;
}

static inline uint32_t getNext4(uint64_t pc, const uint8_t* program) {
    uint8_t byte0 = program[pc];
    uint8_t byte1 = program[pc + 1];
    uint8_t byte2 = program[pc + 2];
    uint8_t byte3 = program[pc + 3];

    return ((uint32_t)byte3 << 24) |
           ((uint32_t)byte2 << 16) |
           ((uint32_t)byte1 << 8)  |
           (uint32_t)byte0;
}

static inline uint64_t getNext8(uint64_t pc, const uint8_t* program) {
    uint8_t byte0 = program[pc];
    uint8_t byte1 = program[pc + 1];
    uint8_t byte2 = program[pc + 2];
    uint8_t byte3 = program[pc + 3];
    uint8_t byte4 = program[pc + 4];
    uint8_t byte5 = program[pc + 5];
    uint8_t byte6 = program[pc + 6];
    uint8_t byte7 = program[pc + 7];

    return ((uint64_t)byte7 << 56) |
           ((uint64_t)byte6 << 48) |
           ((uint64_t)byte5 << 40) |
           ((uint64_t)byte4 << 32) |
           ((uint64_t)byte3 << 24) |
           ((uint64_t)byte2 << 16) |
           ((uint64_t)byte1 << 8)  |
           (uint64_t)byte0;
}

LumiVM* lumiCreateVM(void) {
    LumiVM* vm = nullptr;
    CFrame* cframes = nullptr;
    uint8_t* data = nullptr;

    vm = calloc(1, sizeof(LumiVM));
    if (vm == nullptr) {
        goto cleanup;
    }

    cframes = calloc(CFRAME_COUNT, sizeof(CFrame));
    if (cframes == nullptr) {
        goto cleanup;
    }


    data = calloc(DSTACK_BYTE_COUNT, sizeof(uint8_t));
    if (data == nullptr) {
        goto cleanup;
    }

    vm->cstack.cframes = cframes;
    vm->dstack.data = data;

    return vm;

    cleanup:
    if (data != nullptr) free(data);
    if (cframes != nullptr) free(cframes);
    if (vm != nullptr) free(vm);
    return nullptr;
}

void lumiDestroyVM(LumiVM* vm) {
    if (vm == nullptr) {
        return;
    }

    free(vm->dstack.data);
    free(vm->cstack.cframes);
    free(vm);
}

__attribute__((noinline))
int32_t lumiRunVM(LumiVM* vm, const uint8_t* program, uint64_t program_size) {
    if (vm == nullptr) {
        return EX_STATE_ERR;
    }

    if (program == nullptr) {
        return EX_PROGRAM_ERR;
    }

    static const void* dispatch_table[256] = {
        [0 ... 255] = &&do_invalid,

        [OP_NOP] = &&do_nop,
        [OP_HALT] = &&do_halt,
        [OP_CALL] = &&do_call,
        [OP_RET] = &&do_ret,
        [OP_LOAD] = &&do_load,
        [OP_LOADI] = &&do_loadi,
        [OP_ADD] = &&do_add,
        [OP_ADDI] = &&do_addi,
    };

    dispatch:
    {
        if (vm->pc >= program_size) {
            return EX_ABRUPT_END;
        }

         goto *dispatch_table[program[vm->pc]];
    }

    do_invalid:
    {
        return EX_INV_INS;
    }

    do_nop:
    {
        vm->pc += INS_NOP_WIDTH;

        goto dispatch;
    }

    do_halt:
    {
        return vm->cstack.accumulator;
    }

    do_call:
    {
        if (!hasNext(vm->pc, program_size, INS_CALL_WIDTH)) {
            return EX_ABRUPT_END;
        }

        if (vm->cstack.fp + 1 >= CFRAME_COUNT) {
            return EX_STACK_OF;
        }

        uint8_t start_reg = getNext(vm->pc + INS_CALL_SREG_OFFSET, program);
        uint8_t end_reg = getNext(vm->pc + INS_CALL_EREG_OFFSET, program);

        if (start_reg > end_reg) {
            return EX_INV_INS;
        }

        uint64_t jmp_pc = getNext8(vm->pc + INS_CALL_PC_OFFSET, program);
        uint64_t ret_pc = vm->pc + INS_CALL_WIDTH;

        uint64_t caller_fp = vm->cstack.fp++;
        uint64_t callee_fp = vm->cstack.fp;

        vm->cstack.cframes[callee_fp].fp = caller_fp;
        vm->cstack.cframes[callee_fp].pc = ret_pc;

        size_t reg_count = end_reg - start_reg + 1;

        uint64_t* out_regs = vm->cstack.cframes[caller_fp].registers + start_reg;
        uint64_t* in_regs = vm->cstack.cframes[callee_fp].registers;

        memcpy(in_regs, out_regs, reg_count * sizeof(uint64_t));

        vm->pc = jmp_pc;

        goto dispatch;
    }

    do_ret:
    {
        if (vm->cstack.fp == 0) {
            return EX_ILL_INS;
        }

        vm->pc = vm->cstack.cframes[vm->cstack.fp].pc;
        vm->cstack.fp = vm->cstack.cframes[vm->cstack.fp].fp;

        goto dispatch;
    }

    do_load:
    {
        goto dispatch;
    }

    do_loadi:
    {
        goto dispatch;
    }

    do_add:
    {
        goto dispatch;
    }

    do_addi:
    {
        goto dispatch;
    }
}
