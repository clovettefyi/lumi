/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org.
 */

#include "lumi_vm.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

#define CTC constexpr static

#define INS_REG_LAYOUT(name) \
    CTC uint64_t INS_##name##_WIDTH = 1 + 1; \
    CTC uint64_t INS_##name##_REG_OFFSET = 1;

#define INS_IMM_LAYOUT(name, size) \
    CTC uint64_t INS_##name##_WIDTH = 1 + (size); \
    CTC uint64_t INS_##name##_IMM_OFFSET = 1;

#define INS_RI_LAYOUT(name) \
    INS_REG_LAYOUT(name) \
    INS_IMM_LAYOUT(name##I_B, 1) \
    INS_IMM_LAYOUT(name##I_W, 2) \
    INS_IMM_LAYOUT(name##I_DW, 4) \
    INS_IMM_LAYOUT(name##I_QW, 8)

#define INS_RI_LABEL(name, lower_name) \
    [OP_##name] = &&do_##lower_name, \
    [OP_##name##I_B] = &&do_##lower_name##i_b, \
    [OP_##name##I_W] = &&do_##lower_name##i_w, \
    [OP_##name##I_DW] = &&do_##lower_name##i_dw, \
    [OP_##name##I_QW] = &&do_##lower_name##i_qw, \


#define CHECK_PROGRAM(name) \
    if (!hasNext(vm->pc, program_size, INS_##name##_WIDTH)) { \
        return EX_ABRUPT_END; \
    }

#define INS_REG_DO(name, lower_name, op) \
    do_##lower_name: \
    { \
        CHECK_PROGRAM(name) \
        uint8_t reg = getNext(vm->pc + INS_##name##_REG_OFFSET, program); \
        vm->cstack.accumulator op##= getCurrentCFrame(vm)->registers[reg]; \
        vm->pc += INS_##name##_WIDTH; \
        goto dispatch; \
    }

#define INS_IMM_DO(name, lower_name, suffix, lower_suffix, fetch_fn, op) \
    do_##lower_name##i_##lower_suffix: \
    { \
        CHECK_PROGRAM(name##I_##suffix) \
        auto imm = fetch_fn(vm->pc + INS_##name##I_##suffix##_IMM_OFFSET, program); \
        vm->cstack.accumulator op##= imm; \
        vm->pc += INS_##name##I_##suffix##_WIDTH; \
        goto dispatch; \
    }

#define INS_RI_DO(name, lower_name, op) \
    INS_REG_DO(name, lower_name, op) \
    INS_IMM_DO(name, lower_name, B, b, getNext, op) \
    INS_IMM_DO(name, lower_name, W, w, getNext2, op) \
    INS_IMM_DO(name, lower_name, DW, dw, getNext4, op) \
    INS_IMM_DO(name, lower_name, QW, qw, getNext8, op)

CTC size_t CFRAME_COUNT = 128;
CTC size_t DSTACK_BYTE_COUNT = 1024 * 1024;

CTC uint64_t INS_NOP_WIDTH = 1;

// CTC uint64_t INS_HALT_WIDTH = 1;

CTC uint64_t INS_CALL_WIDTH = 1 + 1 + 1 + 8;
CTC uint64_t INS_CALL_SREG_OFFSET = 1;
CTC uint64_t INS_CALL_EREG_OFFSET = 2;
CTC uint64_t INS_CALL_PC_OFFSET = 3;

// CTC uint64_t INS_RET_WIDTH = 1;

INS_REG_LAYOUT(STR)

INS_RI_LAYOUT(LOAD)
INS_RI_LAYOUT(ADD)
INS_RI_LAYOUT(SUB)
INS_RI_LAYOUT(MUL)

CTC uint64_t INS_JMP_WIDTH = 1 + 8;
CTC uint64_t INS_JMP_PC_OFFSET = 1;

CTC uint64_t INS_JNZ_WIDTH = 1 + 8;
CTC uint64_t INS_JNZ_PC_OFFSET = 1;

static inline bool hasNext(uint64_t pc, uint64_t program_size, uint64_t bytes) {
    if (pc + bytes > program_size) return false;
    return true;
}

static inline uint8_t getNext(uint64_t pc, const uint8_t* program) {
    return program[pc];
}

static inline uint16_t getNext2(uint64_t pc, const uint8_t* program) {
    uint16_t val;
    memcpy(&val, &program[pc], sizeof(uint16_t));
    return val;
}

static inline uint32_t getNext4(uint64_t pc, const uint8_t* program) {
    uint32_t val;
    memcpy(&val, &program[pc], sizeof(uint32_t));
    return val;
}

static inline uint64_t getNext8(uint64_t pc, const uint8_t* program) {
    uint64_t val;
    memcpy(&val, &program[pc], sizeof(uint64_t));
    return val;
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

        INS_RI_LABEL(LOAD, load)

        [OP_STR] = &&do_str,

        INS_RI_LABEL(ADD, add)
        INS_RI_LABEL(SUB, sub)
        INS_RI_LABEL(MUL, mul)

        [OP_JMP] = &&do_jmp,
        [OP_JNZ] = &&do_jnz,
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
        return EX_ILL_INS;
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

    INS_RI_DO(LOAD, load,)

    do_str:
    {
        CHECK_PROGRAM(STR)

        uint8_t reg = getNext(vm->pc + INS_STR_REG_OFFSET, program);
        LumiVMCFrame* cf = getCurrentCFrame(vm);

        cf->registers[reg] = vm->cstack.accumulator;

        vm->pc += INS_STR_WIDTH;
        goto dispatch;
    }

    INS_RI_DO(ADD, add, +)
    INS_RI_DO(SUB, sub, -)
    INS_RI_DO(MUL, mul, *)

    do_jmp:
    {
        CHECK_PROGRAM(JMP)

        uint64_t pc = getNext8(vm->pc + INS_JMP_PC_OFFSET, program);
        vm->pc = pc;

        goto dispatch;
    }

    do_jnz:
    {
        CHECK_PROGRAM(JNZ)

        if (vm->cstack.accumulator == 0) {
            vm->pc += INS_JNZ_WIDTH;
            goto dispatch;
        }

        uint64_t pc = getNext8(vm->pc + INS_JNZ_PC_OFFSET, program);
        vm->pc = pc;

        goto dispatch;
    }
}
