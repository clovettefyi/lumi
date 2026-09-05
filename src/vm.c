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

#define CTC constexpr static

#define DEFINE_INS_REG(name) \
    CTC uint64_t INS_##name##_WIDTH = 1 + 1; \
    CTC uint64_t INS_##name##_REG_OFFSET = 1;

#define DEFINE_INS_IMM(name, size) \
    CTC uint64_t INS_##name##_WIDTH = 1 + (size); \
    CTC uint64_t INS_##name##_IMM_OFFSET = 1;

#define DEFINE_INS_FAMILY(name) \
    DEFINE_INS_REG(name) \
    DEFINE_INS_IMM(name##I_B, 1) \
    DEFINE_INS_IMM(name##I_W, 2) \
    DEFINE_INS_IMM(name##I_DW, 4) \
    DEFINE_INS_IMM(name##I_QW, 8)

#define CHECK_PROGRAM(name) \
    if (!hasNext(vm->pc, program_size, INS_##name##_WIDTH)) { \
        return EX_ABRUPT_END; \
    }

CTC size_t CFRAME_COUNT = 128;
CTC size_t DSTACK_BYTE_COUNT = 1024 * 1024;

CTC uint64_t INS_NOP_WIDTH = 1;

// CTC uint64_t INS_HALT_WIDTH = 1;

CTC uint64_t INS_CALL_WIDTH = 1 + 1 + 1 + 8;
CTC uint64_t INS_CALL_SREG_OFFSET = 1;
CTC uint64_t INS_CALL_EREG_OFFSET = 2;
CTC uint64_t INS_CALL_PC_OFFSET = 3;

// CTC uint64_t INS_RET_WIDTH = 1;

DEFINE_INS_REG(STR)

DEFINE_INS_FAMILY(LOAD)
DEFINE_INS_FAMILY(ADD)
DEFINE_INS_FAMILY(SUB)
DEFINE_INS_FAMILY(MUL)

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

static inline LumiVMCFrame* getCFrame(LumiVM* vm, uint64_t fp) {
    return vm->cstack.cframes + fp;
}

static inline LumiVMCFrame* getCurrentCFrame(LumiVM* vm) {
    return vm->cstack.cframes + vm->cstack.fp;
}

LumiVM* lumiCreateVM(void) {
    LumiVM* vm = nullptr;
    LumiVMCFrame* cframes = nullptr;
    uint8_t* data = nullptr;

    vm = calloc(1, sizeof(LumiVM));
    if (vm == nullptr) {
        goto cleanup;
    }

    cframes = calloc(CFRAME_COUNT, sizeof(LumiVMCFrame));
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

uint64_t lumiVMGetAccumulator(LumiVM* vm) {
    return vm->cstack.accumulator;
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
        [OP_LOADI_B] = &&do_loadi_b,
        [OP_LOADI_W] = &&do_loadi_w,
        [OP_LOADI_DW] = &&do_loadi_dw,
        [OP_LOADI_QW] = &&do_loadi_qw,

        [OP_STR] = &&do_str,

        [OP_ADD] = &&do_add,
        [OP_ADDI_B] = &&do_addi_b,
        [OP_ADDI_W] = &&do_addi_w,
        [OP_ADDI_DW] = &&do_addi_dw,
        [OP_ADDI_QW] = &&do_addi_qw,
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
        CHECK_PROGRAM(CALL)

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

        LumiVMCFrame* caller_cf = getCFrame(vm, vm->cstack.fp++);
        LumiVMCFrame* callee_cf = getCurrentCFrame(vm);

        callee_cf->pc = ret_pc;

        size_t reg_count = end_reg - start_reg + 1;

        uint64_t* out_regs = caller_cf->registers + start_reg;
        uint64_t* in_regs = callee_cf->registers;

        memcpy(in_regs, out_regs, reg_count * sizeof(uint64_t));

        vm->pc = jmp_pc;

        goto dispatch;
    }

    do_ret:
    {
        if (vm->cstack.fp == 0) {
            return EX_ILL_INS;
        }

        vm->pc = getCurrentCFrame(vm)->pc;
        vm->cstack.fp--;

        goto dispatch;
    }

    do_load:
    {
        CHECK_PROGRAM(LOAD)

        uint8_t reg = getNext(vm->pc + INS_LOAD_REG_OFFSET, program);
        vm->cstack.accumulator = getCurrentCFrame(vm)->registers[reg];

        vm->pc += INS_LOAD_WIDTH;
        goto dispatch;
    }

    do_loadi_b:
    {
        CHECK_PROGRAM(LOADI_B)

        uint8_t imm = getNext(vm->pc + INS_LOADI_B_IMM_OFFSET, program);
        vm->cstack.accumulator = imm;

        vm->pc += INS_LOADI_B_WIDTH;
        goto dispatch;
    }

    do_loadi_w:
    {
        CHECK_PROGRAM(LOADI_W)

        uint16_t imm = getNext2(vm->pc + INS_LOADI_W_IMM_OFFSET, program);
        vm->cstack.accumulator = imm;

        vm->pc += INS_LOADI_W_WIDTH;
        goto dispatch;
    }

    do_loadi_dw:
    {
        CHECK_PROGRAM(LOADI_DW)

        uint32_t imm = getNext4(vm->pc + INS_LOADI_DW_IMM_OFFSET, program);
        vm->cstack.accumulator = imm;

        vm->pc += INS_LOADI_DW_WIDTH;
        goto dispatch;
    }

    do_loadi_qw:
    {
        CHECK_PROGRAM(LOADI_QW)

        uint64_t imm = getNext8(vm->pc + INS_LOADI_QW_IMM_OFFSET, program);
        vm->cstack.accumulator = imm;

        vm->pc += INS_LOADI_QW_WIDTH;
        goto dispatch;
    }

    do_str:
    {
        CHECK_PROGRAM(STR)

        uint8_t reg = getNext(vm->pc + INS_STR_REG_OFFSET, program);
        LumiVMCFrame* cf = getCurrentCFrame(vm);

        cf->registers[reg] = vm->cstack.accumulator;

        vm->pc += INS_STR_WIDTH;
        goto dispatch;
    }

    do_add:
    {
        CHECK_PROGRAM(ADD)

        uint8_t reg = getNext(vm->pc + INS_ADD_REG_OFFSET, program);
        vm->cstack.accumulator += getCurrentCFrame(vm)->registers[reg];

        vm->pc += INS_ADD_WIDTH;
        goto dispatch;
    }

    do_addi_b:
    {
        CHECK_PROGRAM(ADDI_B)

        uint8_t imm = getNext(vm->pc + INS_ADDI_B_IMM_OFFSET, program);
        vm->cstack.accumulator += imm;

        vm->pc += INS_ADDI_B_WIDTH;
        goto dispatch;
    }

    do_addi_w:
    {
        CHECK_PROGRAM(ADDI_W)

        uint16_t imm = getNext2(vm->pc + INS_ADDI_W_IMM_OFFSET, program);
        vm->cstack.accumulator += imm;

        vm->pc += INS_ADDI_W_WIDTH;
        goto dispatch;
    }

    do_addi_dw:
    {
        CHECK_PROGRAM(ADDI_DW)

        uint32_t imm = getNext4(vm->pc + INS_ADDI_DW_IMM_OFFSET, program);
        vm->cstack.accumulator += imm;

        vm->pc += INS_ADDI_DW_WIDTH;
        goto dispatch;
    }

    do_addi_qw:
    {
        CHECK_PROGRAM(ADDI_QW)

        uint64_t imm = getNext8(vm->pc + INS_ADDI_QW_IMM_OFFSET, program);
        vm->cstack.accumulator += imm;

        vm->pc += INS_ADDI_QW_WIDTH;
        goto dispatch;
    }
}
