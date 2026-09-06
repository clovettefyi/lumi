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

/*
 -- MACROS --
*/

#define CTC constexpr static

#define INS_REG_LAYOUT(name) \
    CTC uint64_t INS_##name##_WIDTH = 1 + 1 + 1; \
    CTC uint64_t INS_##name##_DST_OFFSET = 1; \
    CTC uint64_t INS_##name##_SRC_OFFSET = 2;

#define INS_IMM_LAYOUT(name, size) \
    CTC uint64_t INS_##name##_WIDTH = 1 + 1 + (size); \
    CTC uint64_t INS_##name##_DST_OFFSET = 1; \
    CTC uint64_t INS_##name##_IMM_OFFSET = 2;

#define CHECK_PROGRAM(name) \
    if (!hasNext(vm->pc, program_size, INS_##name##_WIDTH)) { \
        return EX_ABRUPT_END; \
    }

#define INS_REG_DO(name, op) \
    uint8_t dst = getNext(vm->pc + INS_##name##_DST_OFFSET, program); \
    uint8_t src = getNext(vm->pc + INS_##name##_SRC_OFFSET, program); \
    LumiVMCFrame* cf = getCurrentCFrame(vm); \
    cf->registers[dst] op##= cf->registers[src]; \
    vm->pc += INS_##name##_WIDTH; \

#define INS_IMM_DO(name, fetch_fn, op) \
    uint8_t dst = getNext(vm->pc + INS_##name##_DST_OFFSET, program); \
    auto imm = fetch_fn(vm->pc + INS_##name##_IMM_OFFSET, program); \
    LumiVMCFrame* cf = getCurrentCFrame(vm); \
    cf->registers[dst] op##= imm; \
    vm->pc += INS_##name##_WIDTH; \

/*
 -- VM CONSTANTS --
*/

CTC size_t CFRAME_COUNT = 128;
CTC size_t DSTACK_BYTE_COUNT = 1024 * 1024;

/*
 -- INSTRUCTION LAYOUT --
*/

CTC uint64_t INS_NOP_WIDTH = 1;

// CTC uint64_t INS_HALT_WIDTH = 1;

CTC uint64_t INS_CALL_WIDTH = 1 + 1 + 1 + 8;
CTC uint64_t INS_CALL_SREG_OFFSET = 1;
CTC uint64_t INS_CALL_EREG_OFFSET = 2;
CTC uint64_t INS_CALL_PC_OFFSET = 3;

CTC uint64_t INS_RET_WIDTH = 1 + 1;
CTC uint64_t INS_RET_REG_OFFSET = 1;

INS_REG_LAYOUT(MOV);

INS_IMM_LAYOUT(LOAD_B, 1);
INS_IMM_LAYOUT(LOAD_W, 2);
INS_IMM_LAYOUT(LOAD_DW, 4);
INS_IMM_LAYOUT(LOAD_QW, 8);

INS_REG_LAYOUT(ADD);
INS_IMM_LAYOUT(ADD_B, 1);
INS_IMM_LAYOUT(ADD_W, 2);
INS_IMM_LAYOUT(ADD_DW, 4);
INS_IMM_LAYOUT(ADD_QW, 8);

INS_REG_LAYOUT(SUB);
INS_IMM_LAYOUT(SUB_B, 1);
INS_IMM_LAYOUT(SUB_W, 2);
INS_IMM_LAYOUT(SUB_DW, 4);
INS_IMM_LAYOUT(SUB_QW, 8);

INS_REG_LAYOUT(MUL);
INS_IMM_LAYOUT(MUL_B, 1);
INS_IMM_LAYOUT(MUL_W, 2);
INS_IMM_LAYOUT(MUL_DW, 4);
INS_IMM_LAYOUT(MUL_QW, 8);

CTC uint64_t INS_JMP_WIDTH = 1 + 8;
CTC uint64_t INS_JMP_PC_OFFSET = 1;

CTC uint64_t INS_JNZ_WIDTH = 1 + 1 + 8;
CTC uint64_t INS_JNZ_REG_OFFSET = 1;
CTC uint64_t INS_JNZ_PC_OFFSET = 2;

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

        [OP_MOV] = &&do_mov,

        [OP_LOAD_B] = &&do_load_b,
        [OP_LOAD_W] = &&do_load_w,
        [OP_LOAD_DW] = &&do_load_dw,
        [OP_LOAD_QW] = &&do_load_qw,

        [OP_ADD] = &&do_add,
        [OP_ADD_B] = &&do_add_b,
        [OP_ADD_W] = &&do_add_w,
        [OP_ADD_DW] = &&do_add_dw,
        [OP_ADD_QW] = &&do_add_qw,

        [OP_SUB] = &&do_sub,
        [OP_SUB_B] = &&do_sub_b,
        [OP_SUB_W] = &&do_sub_w,
        [OP_SUB_DW] = &&do_sub_dw,
        [OP_SUB_QW] = &&do_sub_qw,

        [OP_MUL] = &&do_mul,
        [OP_MUL_B] = &&do_mul_b,
        [OP_MUL_W] = &&do_mul_w,
        [OP_MUL_DW] = &&do_mul_dw,
        [OP_MUL_QW] = &&do_mul_qw,

        [OP_JMP] = &&do_jmp,
        [OP_JNZ] = &&do_jnz,
    };

    dispatch: {
        if (vm->pc >= program_size) {
            return EX_ABRUPT_END;
        }

         goto *dispatch_table[program[vm->pc]];
    }

    do_invalid: {
        return EX_ILL_INS;
    }

    do_nop: {
        vm->pc += INS_NOP_WIDTH;

        goto dispatch;
    }

    do_halt: {
        return getCurrentCFrame(vm)->registers[0];
    }

    do_call: {
        CHECK_PROGRAM(CALL);

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

    do_ret: {
        if (vm->cstack.fp == 0) {
            return EX_ILL_INS;
        }

        CHECK_PROGRAM(RET);

        uint64_t* ret_reg = getCurrentCFrame(vm)->registers + getNext(vm->pc + INS_RET_REG_OFFSET, program);

        vm->pc = getCurrentCFrame(vm)->pc;
        vm->cstack.fp--;

        memcpy(getCurrentCFrame(vm)->registers, ret_reg, sizeof(uint64_t));

        goto dispatch;
    }

    do_mov: {
        CHECK_PROGRAM(MOV);
        INS_REG_DO(MOV,);
        goto dispatch;
    }

    do_load_b: {
        CHECK_PROGRAM(LOAD_B);
        INS_IMM_DO(LOAD_B, getNext,);
        goto dispatch;
    }

    do_load_w: {
        CHECK_PROGRAM(LOAD_W);
        INS_IMM_DO(LOAD_W, getNext2,);
        goto dispatch;
    }

    do_load_dw: {
        CHECK_PROGRAM(LOAD_DW);
        INS_IMM_DO(LOAD_DW, getNext4,);
        goto dispatch;
    }

    do_load_qw: {
        CHECK_PROGRAM(LOAD_QW);
        INS_IMM_DO(LOAD_QW, getNext8,);
        goto dispatch;
    }

    do_add: {
        CHECK_PROGRAM(ADD);
        INS_REG_DO(ADD, +);
        goto dispatch;
    }

    do_add_b: {
        CHECK_PROGRAM(ADD_B);
        INS_IMM_DO(ADD_B, getNext, +);
        goto dispatch;
    }

    do_add_w: {
        CHECK_PROGRAM(ADD_W);
        INS_IMM_DO(ADD_W, getNext2, +);
        goto dispatch;
    }

    do_add_dw: {
        CHECK_PROGRAM(ADD_DW);
        INS_IMM_DO(ADD_DW, getNext4, +);
        goto dispatch;
    }

    do_add_qw: {
        CHECK_PROGRAM(ADD_QW);
        INS_IMM_DO(ADD_QW, getNext8, +);
        goto dispatch;
    }

    do_sub: {
        CHECK_PROGRAM(SUB);
        INS_REG_DO(SUB, -);
        goto dispatch;
    }

    do_sub_b: {
        CHECK_PROGRAM(SUB_B);
        INS_IMM_DO(SUB_B, getNext, -);
        goto dispatch;
    }

    do_sub_w: {
        CHECK_PROGRAM(SUB_W);
        INS_IMM_DO(SUB_W, getNext2, -);
        goto dispatch;
    }

    do_sub_dw: {
        CHECK_PROGRAM(SUB_DW);
        INS_IMM_DO(SUB_DW, getNext4, -);
        goto dispatch;
    }

    do_sub_qw: {
        CHECK_PROGRAM(SUB_QW);
        INS_IMM_DO(SUB_QW, getNext8, -);
        goto dispatch;
    }

    do_mul: {
        CHECK_PROGRAM(MUL);
        INS_REG_DO(MUL, *);
        goto dispatch;
    }

    do_mul_b: {
        CHECK_PROGRAM(MUL_B);
        INS_IMM_DO(MUL_B, getNext, *);
        goto dispatch;
    }

    do_mul_w: {
        CHECK_PROGRAM(MUL_W);
        INS_IMM_DO(MUL_W, getNext2, *);
        goto dispatch;
    }

    do_mul_dw: {
        CHECK_PROGRAM(MUL_DW);
        INS_IMM_DO(MUL_DW, getNext4, *);
        goto dispatch;
    }

    do_mul_qw: {
        CHECK_PROGRAM(MUL_QW);
        INS_IMM_DO(MUL_QW, getNext8, *);
    }

    do_jmp: {
        CHECK_PROGRAM(JMP);

        uint64_t pc = getNext8(vm->pc + INS_JMP_PC_OFFSET, program);
        vm->pc = pc;

        goto dispatch;
    }

    do_jnz: {
        CHECK_PROGRAM(JNZ);

        uint8_t reg = getNext(vm->pc + INS_JNZ_REG_OFFSET, program);

        if (getCurrentCFrame(vm)->registers[reg] == 0) {
            vm->pc += INS_JNZ_WIDTH;
            goto dispatch;
        }

        uint64_t pc = getNext8(vm->pc + INS_JNZ_PC_OFFSET, program);
        vm->pc = pc;

        goto dispatch;
    }
}
