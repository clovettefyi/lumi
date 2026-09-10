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

CTC uint64_t INS_CALLR_WIDTH = 1 + 1 + 1 + 1;
CTC uint64_t INS_CALLR_SREG_OFFSET = 1;
CTC uint64_t INS_CALLR_EREG_OFFSET = 2;
CTC uint64_t INS_CALLR_REG_OFFSET = 3;

CTC uint64_t INS_RET_WIDTH = 1 + 1;
CTC uint64_t INS_RET_REG_OFFSET = 1;

CTC uint64_t INS_MOV_WIDTH = 1 + 1 + 1;
CTC uint64_t INS_MOV_DST_OFFSET = 1;
CTC uint64_t INS_MOV_SRC_OFFSET = 2;

CTC uint64_t INS_LOAD_WIDTH = 1 + 1 + 8;
CTC uint64_t INS_LOAD_DST_OFFSET = 1;
CTC uint64_t INS_LOAD_IMM_OFFSET = 2;

#define INS_ARITH_REG_LAYOUT(name) \
    CTC uint64_t INS_##name##_WIDTH = 1 + 1 + 1 +1; \
    CTC uint64_t INS_##name##_DST_OFFSET = 1; \
    CTC uint64_t INS_##name##_SRC1_OFFSET = 2; \
    CTC uint64_t INS_##name##_SRC2_OFFSET = 3;

#define INS_ARITH_IMM_LAYOUT(name) \
    CTC uint64_t INS_##name##_WIDTH = 1 + 1 + 1 + 8; \
    CTC uint64_t INS_##name##_DST_OFFSET = 1; \
    CTC uint64_t INS_##name##_SRC_OFFSET = 2; \
    CTC uint64_t INS_##name##_IMM_OFFSET = 3;

INS_ARITH_REG_LAYOUT(ADD);
INS_ARITH_IMM_LAYOUT(ADDI);

INS_ARITH_REG_LAYOUT(SUB);
INS_ARITH_IMM_LAYOUT(SUBI);

INS_ARITH_REG_LAYOUT(MUL);
INS_ARITH_IMM_LAYOUT(MULI);

CTC uint64_t INS_JMP_WIDTH = 1 + 8;
CTC uint64_t INS_JMP_PC_OFFSET = 1;

#define INS_BRANCH_REG_LAYOUT(name) \
    CTC uint64_t INS_##name##_WIDTH = 1 + 1 + 1 + 8; \
    CTC uint64_t INS_##name##_SRC1_OFFSET = 1; \
    CTC uint64_t INS_##name##_SRC2_OFFSET = 2; \
    CTC uint64_t INS_##name##_OFFSET_OFFSET = 3;

#define INS_BRANCH_IMM_LAYOUT(name) \
    CTC uint64_t INS_##name##_WIDTH = 1 + 1 + 8 + 8; \
    CTC uint64_t INS_##name##_SRC_OFFSET = 1; \
    CTC uint64_t INS_##name##_IMM_OFFSET = 2; \
    CTC uint64_t INS_##name##_OFFSET_OFFSET = 10;

INS_BRANCH_REG_LAYOUT(BEQ);
INS_BRANCH_IMM_LAYOUT(BEQI);

INS_BRANCH_REG_LAYOUT(BNE);
INS_BRANCH_IMM_LAYOUT(BNEI);

INS_BRANCH_REG_LAYOUT(BGT);
INS_BRANCH_IMM_LAYOUT(BGTI);

INS_BRANCH_REG_LAYOUT(BLT);
INS_BRANCH_IMM_LAYOUT(BLTI);

INS_BRANCH_REG_LAYOUT(BGE);
INS_BRANCH_IMM_LAYOUT(BGEI);

INS_BRANCH_REG_LAYOUT(BLE);
INS_BRANCH_IMM_LAYOUT(BLEI);

/*
 -- HELPER FUNCTIONS --
*/

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

/*
 -- CREATE VM --
*/

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

/*
 -- DESTROY VM --
*/

void lumiDestroyVM(LumiVM* vm) {
    if (vm == nullptr) {
        return;
    }

    free(vm->dstack.data);
    free(vm->cstack.cframes);
    free(vm);
}

/*
 -- RUN VM --
*/

__attribute__((noinline))
uint8_t lumiRunVM(LumiVM* vm, const uint8_t* program, uint64_t program_size) {
    if (vm == nullptr) {
        return EX_SIG_ERR + SIG_VM_ERR;
    }

    if (program == nullptr) {
        return EX_SIG_ERR + SIG_PROG_ERR;
    }

    static const void* dispatch_table[256] = {
        [0 ... 255] = &&do_invalid,

        [OP_NOP] = &&do_nop,
        [OP_HALT] = &&do_halt,

        [OP_CALL] = &&do_call,
        [OP_CALLR] = &&do_callr,
        [OP_RET] = &&do_ret,

        [OP_MOV] = &&do_mov,
        [OP_LOAD] = &&do_load,

        [OP_ADD] = &&do_add,
        [OP_ADDI] = &&do_addi,

        [OP_SUB] = &&do_sub,
        [OP_SUBI] = &&do_subi,

        [OP_MUL] = &&do_mul,
        [OP_MULI] = &&do_muli,

        [OP_JMP] = &&do_jmp,

        [OP_BEQ] = &&do_beq,
        [OP_BEQI] = &&do_beqi,

        [OP_BNE] = &&do_bne,
        [OP_BNEI] = &&do_bnei,

        [OP_BGT] = &&do_bgt,
        [OP_BGTI] = &&do_bgti,

        [OP_BLT] = &&do_blt,
        [OP_BLTI] = &&do_blti,

        [OP_BGE] = &&do_bge,
        [OP_BGEI] = &&do_bgei,

        [OP_BLE] = &&do_ble,
        [OP_BLEI] = &&do_blei,
    };

    #define CHECK_PROGRAM(name) \
        if (!hasNext(vm->pc, program_size, INS_##name##_WIDTH)) { \
            return EX_SIG_ERR + SIG_SEGV_PC; \
        }

    dispatch: {
        if (vm->pc >= program_size) {
            return EX_SIG_ERR + SIG_SEGV_PC;
        }

         goto *dispatch_table[program[vm->pc]];
    }

    /*
     -- INSTRUCTIONS --
    */

    do_invalid: {
        return EX_SIG_ERR + SIG_ILL;
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
            return EX_SIG_ERR + SIG_SEGV_SOF;
        }

        uint8_t start_reg = getNext(vm->pc + INS_CALL_SREG_OFFSET, program);
        uint8_t end_reg = getNext(vm->pc + INS_CALL_EREG_OFFSET, program);

        if (start_reg > end_reg) {
            return EX_SIG_ERR + SIG_ILL;
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

    do_callr: {
        CHECK_PROGRAM(CALLR);

        if (vm->cstack.fp + 1 >= CFRAME_COUNT) {
            return EX_SIG_ERR + SIG_SEGV_SOF;
        }

        uint8_t start_reg = getNext(vm->pc + INS_CALLR_SREG_OFFSET, program);
        uint8_t end_reg = getNext(vm->pc + INS_CALLR_EREG_OFFSET, program);

        if (start_reg > end_reg) {
            return EX_SIG_ERR + SIG_ILL;
        }

        uint8_t reg = getNext(vm->pc + INS_CALLR_REG_OFFSET, program);
        uint64_t jmp_pc = getCurrentCFrame(vm)->registers[reg];
        uint64_t ret_pc = vm->pc + INS_CALLR_WIDTH;

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
            return EX_SIG_ERR + SIG_ILL;
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

        uint8_t dst = getNext(vm->pc + INS_MOV_DST_OFFSET, program);
        uint8_t src = getNext(vm->pc + INS_MOV_SRC_OFFSET, program);
        LumiVMCFrame* cf = getCurrentCFrame(vm);

        cf->registers[dst] = cf->registers[src];

        vm->pc += INS_MOV_WIDTH;
        goto dispatch;
    }

    do_load: {
        CHECK_PROGRAM(LOAD);

        uint8_t dst = getNext(vm->pc + INS_LOAD_DST_OFFSET, program);
        uint64_t imm = getNext8(vm->pc + INS_LOAD_IMM_OFFSET, program);
        LumiVMCFrame* cf = getCurrentCFrame(vm);

        cf->registers[dst] = imm;

        vm->pc += INS_LOAD_WIDTH;
        goto dispatch;
    }

    #define INS_ARITH_REG_DO(name, op) \
        uint8_t dst = getNext(vm->pc + INS_##name##_DST_OFFSET, program); \
        uint8_t src1 = getNext(vm->pc + INS_##name##_SRC1_OFFSET, program); \
        uint8_t src2 = getNext(vm->pc + INS_##name##_SRC2_OFFSET, program); \
        LumiVMCFrame* cf = getCurrentCFrame(vm); \
        cf->registers[dst] = cf->registers[src1] op cf->registers[src2]; \
        vm->pc += INS_##name##_WIDTH; \

    #define INS_ARITH_IMM_DO(name, op) \
        uint8_t dst = getNext(vm->pc + INS_##name##_DST_OFFSET, program); \
        uint8_t src = getNext(vm->pc + INS_##name##_SRC_OFFSET, program); \
        uint64_t imm = getNext8(vm->pc + INS_##name##_IMM_OFFSET, program); \
        LumiVMCFrame* cf = getCurrentCFrame(vm); \
        cf->registers[dst] = cf->registers[src] op imm; \
        vm->pc += INS_##name##_WIDTH;

    do_add: {
        CHECK_PROGRAM(ADD);
        INS_ARITH_REG_DO(ADD, +);
        goto dispatch;
    }

    do_addi: {
        CHECK_PROGRAM(ADDI);
        INS_ARITH_IMM_DO(ADDI, +);
        goto dispatch;
    }

    do_sub: {
        CHECK_PROGRAM(SUB);
        INS_ARITH_REG_DO(SUB, -);
        goto dispatch;
    }

    do_subi: {
        CHECK_PROGRAM(SUBI);
        INS_ARITH_IMM_DO(SUBI, -);
        goto dispatch;
    }

    do_mul: {
        CHECK_PROGRAM(MUL);
        INS_ARITH_REG_DO(MUL, *);
        goto dispatch;
    }

    do_muli: {
        CHECK_PROGRAM(MULI);
        INS_ARITH_IMM_DO(MULI, *);
        goto dispatch;
    }

    do_jmp: {
        CHECK_PROGRAM(JMP);

        uint64_t pc = getNext8(vm->pc + INS_JMP_PC_OFFSET, program);
        vm->pc = pc;

        goto dispatch;
    }

    #define INS_BRANCH_REG_DO(name, comparitor) \
        uint8_t src1 = getNext(vm->pc + INS_##name##_SRC1_OFFSET, program); \
        uint8_t src2 = getNext(vm->pc + INS_##name##_SRC2_OFFSET, program); \
        int64_t offset = getNext8(vm->pc + INS_##name##_OFFSET_OFFSET, program); \
        LumiVMCFrame* cf = getCurrentCFrame(vm); \
        vm->pc += INS_##name##_WIDTH; \
        if (cf->registers[src1] comparitor cf->registers[src2]) { vm->pc += offset; }

    #define INS_BRANCH_IMM_DO(name, comparitor) \
        uint8_t src1 = getNext(vm->pc + INS_##name##_SRC_OFFSET, program); \
        uint64_t imm = getNext8(vm->pc + INS_##name##_IMM_OFFSET, program); \
        int64_t offset = getNext8(vm->pc + INS_##name##_OFFSET_OFFSET, program); \
        vm->pc += INS_##name##_WIDTH; \
        if (getCurrentCFrame(vm)->registers[src1] comparitor imm) { vm->pc += offset; }

    do_beq: {
        CHECK_PROGRAM(BEQ);
        INS_BRANCH_REG_DO(BEQ, ==);
        goto dispatch;
    }

    do_beqi: {
        CHECK_PROGRAM(BEQI);
        INS_BRANCH_IMM_DO(BEQI, ==);
        goto dispatch;
    }

    do_bne: {
        CHECK_PROGRAM(BNE);
        INS_BRANCH_REG_DO(BNE, !=);
        goto dispatch;
    }

    do_bnei: {
        CHECK_PROGRAM(BNEI);
        INS_BRANCH_IMM_DO(BNEI, !=);
        goto dispatch;
    }

    do_bgt: {
        CHECK_PROGRAM(BGT);
        INS_BRANCH_REG_DO(BGT, >);
        goto dispatch;
    }

    do_bgti: {
        CHECK_PROGRAM(BGTI);
        INS_BRANCH_IMM_DO(BGTI, >);
        goto dispatch;
    }

    do_blt: {
        CHECK_PROGRAM(BLT);
        INS_BRANCH_REG_DO(BLT, <);
        goto dispatch;
    }

    do_blti: {
        CHECK_PROGRAM(BLTI);
        INS_BRANCH_IMM_DO(BLTI, <);
        goto dispatch;
    }

    do_bge: {
        CHECK_PROGRAM(BGE);
        INS_BRANCH_REG_DO(BGE, >=);
        goto dispatch;
    }

    do_bgei: {
        CHECK_PROGRAM(BGEI);
        INS_BRANCH_IMM_DO(BGEI, >=);
        goto dispatch;
    }

    do_ble: {
        CHECK_PROGRAM(BLE);
        INS_BRANCH_REG_DO(BLE, <=);
        goto dispatch;
    }

    do_blei: {
        CHECK_PROGRAM(BLEI);
        INS_BRANCH_IMM_DO(BLEI, <=);
        goto dispatch;
    }
 }
