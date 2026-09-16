/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org.
 */

#include "lprism.h"

#include <inttypes.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

#define CTC constexpr static

CTC size_t GSTACK_SIZE = 1024 * 1024;
CTC uint64_t DATA_SLOTS = GSTACK_SIZE / sizeof(uint64_t);
CTC uint64_t CFRAME_WIDTH = sizeof(PrismCFrame) / sizeof(uint64_t);

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

/*
 -- CREATE VM --
*/

PrismVM* prismCreate(uint8_t* program, uint64_t program_size) {
    PrismVM* vm = nullptr;
    uint64_t* gstack = nullptr;

    vm = malloc(sizeof(PrismVM));
    if (vm == nullptr) {
        goto cleanup;
    }

    gstack = malloc(GSTACK_SIZE);
    if (gstack == nullptr) {
        goto cleanup;
    }

    vm->gstack = gstack;

    vm->cstack.cframes = (PrismCFrame*)gstack;
    vm->cstack.fp = 0;

    vm->dstack.data = gstack + DATA_SLOTS - 1;
    vm->dstack.sp = 0;
    vm->dstack.bp = 0;

    vm->program = program;
    vm->program_size = program_size;
    vm->pc = 0;

    return vm;

    cleanup: {
        if (gstack != nullptr) free(gstack);
        if (vm != nullptr) free(vm);
        return nullptr;
    }
}

/*
 -- DESTROY VM --
*/

void prismDestroy(PrismVM* vm) {
    if (vm == nullptr) {
        return;
    }

    free(vm->gstack);
    free(vm);
}

/*
 -- RUN VM --
*/

__attribute__((noinline))
uint8_t prismStepForward(PrismVM* vm, uint64_t step) {
    if (vm == nullptr) {
        return PRISM_EX_SIG_ERR + PRISM_SIG_VM_ERR;
    }

    if (vm->program == nullptr) {
        return PRISM_EX_SIG_ERR + PRISM_SIG_PROG_ERR;
    }

    // issue: some hoisted variables may cause performance issue
    // worth a review at a later date
    const uint8_t* program = vm->program;
    const uint64_t program_size = vm->program_size;
    uint64_t pc = vm->pc;

    uint64_t fp = vm->cstack.fp;
    PrismCFrame* current_cf = vm->cstack.cframes + fp;

    uint64_t* dstack = vm->dstack.data;
    uint64_t sp = vm->dstack.sp;
    uint64_t bp = vm->dstack.bp;

    static const void* dispatch_table[256] = {
        [0 ... 255] = &&do_invalid,

        [PRISM_OP_NOP] = &&do_nop,

        [PRISM_OP_HALT] = &&do_halt,
        [PRISM_OP_HALTR] = &&do_haltr,
        [PRISM_OP_HALTI] = &&do_halti,

        [PRISM_OP_CALL] = &&do_call,
        [PRISM_OP_CALLR] = &&do_callr,
        [PRISM_OP_RET] = &&do_ret,

        [PRISM_OP_MOV] = &&do_mov,
        [PRISM_OP_LOAD] = &&do_load,

        [PRISM_OP_ADD] = &&do_add,
        [PRISM_OP_ADDI] = &&do_addi,

        [PRISM_OP_SUB] = &&do_sub,
        [PRISM_OP_SUBI] = &&do_subi,

        [PRISM_OP_MUL] = &&do_mul,
        [PRISM_OP_MULI] = &&do_muli,

        [PRISM_OP_JMP] = &&do_jmp,

        [PRISM_OP_BEQ] = &&do_beq,
        [PRISM_OP_BEQI] = &&do_beqi,

        [PRISM_OP_BNE] = &&do_bne,
        [PRISM_OP_BNEI] = &&do_bnei,

        [PRISM_OP_BGT] = &&do_bgt,
        [PRISM_OP_BGTI] = &&do_bgti,

        [PRISM_OP_BLT] = &&do_blt,
        [PRISM_OP_BLTI] = &&do_blti,

        [PRISM_OP_BGE] = &&do_bge,
        [PRISM_OP_BGEI] = &&do_bgei,

        [PRISM_OP_BLE] = &&do_ble,
        [PRISM_OP_BLEI] = &&do_blei,

        [PRISM_OP_ALLOC] = &&do_alloc,
        [PRISM_OP_ALLOCR] = &&do_allocr,
    };

    #define CHECK_PROGRAM(name) \
        if (!hasNext(pc, program_size, PRISM_INS_##name##_WIDTH)) { \
            EXIT_PROGRAM(PRISM_EX_SIG_ERR + PRISM_SIG_SEGV_PC); \
        }

    #define NEXT_INSTRUCTION(name) pc += PRISM_INS_##name##_WIDTH; goto dispatch;

    #define EXIT_PROGRAM(exit_code) \
        vm->cstack.fp = fp; \
        vm->dstack.sp = sp; \
        vm->dstack.bp = bp; \
        vm->pc = pc; \
        return exit_code;

    dispatch: {
        if (pc >= program_size) {
            EXIT_PROGRAM(PRISM_EX_SIG_ERR + PRISM_SIG_SEGV_PC);
        }

        if (step-- == 0) {
            EXIT_PROGRAM(current_cf->registers[0]);
        }

         goto *dispatch_table[program[pc]];
    }

    /*
     -- INSTRUCTIONS --
    */

    do_invalid: {
        EXIT_PROGRAM(PRISM_EX_SIG_ERR + PRISM_SIG_ILL);
    }

    do_nop: {
        NEXT_INSTRUCTION(NOP);
    }

    do_halt: {
        pc += PRISM_INS_HALT_WIDTH;
        EXIT_PROGRAM(current_cf->registers[0]);
    }

    do_haltr: {
        CHECK_PROGRAM(HALTR);

        uint8_t reg = getNext(pc + PRISM_INS_HALTR_REG_OFFSET, program);

        pc += PRISM_INS_HALTR_WIDTH;
        EXIT_PROGRAM(current_cf->registers[reg]);
    }

    do_halti: {
        CHECK_PROGRAM(HALTI);

        uint64_t imm = getNext8(pc + PRISM_INS_HALTI_IMM_OFFSET, program);

        pc += PRISM_INS_HALTI_WIDTH;
        EXIT_PROGRAM(imm);
    }

    do_call: {
        CHECK_PROGRAM(CALL);

        if ((fp + 2) * CFRAME_WIDTH + (sp + 1) > DATA_SLOTS) {
            EXIT_PROGRAM(PRISM_EX_SIG_ERR + PRISM_SIG_SEGV_SOF);
        }

        uint8_t start_reg = getNext(pc + PRISM_INS_CALL_SREG_OFFSET, program);
        uint8_t end_reg = getNext(pc + PRISM_INS_CALL_EREG_OFFSET, program);

        if (start_reg > end_reg) {
            EXIT_PROGRAM(PRISM_EX_SIG_ERR + PRISM_SIG_ILL);
        }

        uint64_t jmp_pc = getNext8(pc + PRISM_INS_CALL_PC_OFFSET, program);
        uint64_t ret_pc = pc + PRISM_INS_CALL_WIDTH;

        PrismCFrame* caller_cf = current_cf;
        fp++;
        current_cf++;
        PrismCFrame* callee_cf = current_cf;

        callee_cf->pc = ret_pc;

        size_t reg_count = end_reg - start_reg + 1;

        uint64_t* out_regs = caller_cf->registers + start_reg;
        uint64_t* in_regs = callee_cf->registers;

        memcpy(in_regs, out_regs, reg_count * sizeof(uint64_t));

        pc = jmp_pc;

        *(dstack - sp++) = bp;
        bp = sp;

        goto dispatch;
    }

    do_callr: {
        CHECK_PROGRAM(CALLR);

        if ((fp + 2) * CFRAME_WIDTH + (sp + 1) > DATA_SLOTS) {
            EXIT_PROGRAM(PRISM_EX_SIG_ERR + PRISM_SIG_SEGV_SOF);
        }

        uint8_t start_reg = getNext(pc + PRISM_INS_CALLR_SREG_OFFSET, program);
        uint8_t end_reg = getNext(pc + PRISM_INS_CALLR_EREG_OFFSET, program);

        if (start_reg > end_reg) {
            EXIT_PROGRAM(PRISM_EX_SIG_ERR + PRISM_SIG_ILL);
        }

        uint8_t reg = getNext(pc + PRISM_INS_CALLR_REG_OFFSET, vm->program);
        uint64_t jmp_pc = current_cf->registers[reg];
        uint64_t ret_pc = pc + PRISM_INS_CALLR_WIDTH;

        PrismCFrame* caller_cf = current_cf;
        fp++;
        current_cf++;
        PrismCFrame* callee_cf = current_cf;

        callee_cf->pc = ret_pc;

        size_t reg_count = end_reg - start_reg + 1;

        uint64_t* out_regs = caller_cf->registers + start_reg;
        uint64_t* in_regs = callee_cf->registers;

        memcpy(in_regs, out_regs, reg_count * sizeof(uint64_t));

        pc = jmp_pc;

        *(dstack - sp++) = bp;
        bp = sp;

        goto dispatch;
    }

    do_ret: {
        if (fp == 0) {
            EXIT_PROGRAM(PRISM_EX_SIG_ERR + PRISM_SIG_ILL);
        }

        CHECK_PROGRAM(RET);

        uint64_t* ret_reg = current_cf->registers + getNext(pc + PRISM_INS_RET_REG_OFFSET, program);

        pc = current_cf->pc;
        fp--;
        current_cf--;

        memcpy(current_cf->registers, ret_reg, sizeof(uint64_t));

        sp = bp - 1;
        bp = *(dstack - sp);

        goto dispatch;
    }

    do_mov: {
        CHECK_PROGRAM(MOV);

        uint8_t dst = getNext(pc + PRISM_INS_MOV_DST_OFFSET, program);
        uint8_t src = getNext(pc + PRISM_INS_MOV_SRC_OFFSET, program);
        PrismCFrame* cf = current_cf;

        cf->registers[dst] = cf->registers[src];

        NEXT_INSTRUCTION(MOV);
    }

    do_load: {
        CHECK_PROGRAM(LOAD);

        uint8_t dst = getNext(pc + PRISM_INS_LOAD_DST_OFFSET, program);
        uint64_t imm = getNext8(pc + PRISM_INS_LOAD_IMM_OFFSET, program);
        PrismCFrame* cf = current_cf;

        cf->registers[dst] = imm;

        NEXT_INSTRUCTION(LOAD);
    }

    #define INS_ARITH_REG_DO(name, op) \
        uint8_t dst = getNext(pc + PRISM_INS_##name##_DST_OFFSET, program); \
        uint8_t src1 = getNext(pc + PRISM_INS_##name##_SRC1_OFFSET, program); \
        uint8_t src2 = getNext(pc + PRISM_INS_##name##_SRC2_OFFSET, program); \
        current_cf->registers[dst] = current_cf->registers[src1] op current_cf->registers[src2]; \

    #define INS_ARITH_IMM_DO(name, op) \
        uint8_t dst = getNext(pc + PRISM_INS_##name##_DST_OFFSET, program); \
        uint8_t src = getNext(pc + PRISM_INS_##name##_SRC_OFFSET, program); \
        uint64_t imm = getNext8(pc + PRISM_INS_##name##_IMM_OFFSET, program); \
        current_cf->registers[dst] = current_cf->registers[src] op imm; \

    do_add: {
        CHECK_PROGRAM(ADD);
        INS_ARITH_REG_DO(ADD, +);
        NEXT_INSTRUCTION(ADD);
    }

    do_addi: {
        CHECK_PROGRAM(ADDI);
        INS_ARITH_IMM_DO(ADDI, +);
        NEXT_INSTRUCTION(ADDI);
    }

    do_sub: {
        CHECK_PROGRAM(SUB);
        INS_ARITH_REG_DO(SUB, -);
        NEXT_INSTRUCTION(SUB);
    }

    do_subi: {
        CHECK_PROGRAM(SUBI);
        INS_ARITH_IMM_DO(SUBI, -);
        NEXT_INSTRUCTION(SUBI);
    }

    do_mul: {
        CHECK_PROGRAM(MUL);
        INS_ARITH_REG_DO(MUL, *);
        NEXT_INSTRUCTION(MUL);
    }

    do_muli: {
        CHECK_PROGRAM(MULI);
        INS_ARITH_IMM_DO(MULI, *);
        NEXT_INSTRUCTION(MULI);
    }

    do_jmp: {
        CHECK_PROGRAM(JMP);

        uint64_t jmp_pc = getNext8(pc + PRISM_INS_JMP_PC_OFFSET, program);
        pc = jmp_pc;

        goto dispatch;
    }

    #define INS_BRANCH_REG_DO(name, comparitor) \
        uint8_t src1 = getNext(pc + PRISM_INS_##name##_SRC1_OFFSET, program); \
        uint8_t src2 = getNext(pc + PRISM_INS_##name##_SRC2_OFFSET, program); \
        int64_t offset = getNext8(pc + PRISM_INS_##name##_OFFSET_OFFSET, program); \
        pc += PRISM_INS_##name##_WIDTH; \
        if (current_cf->registers[src1] comparitor current_cf->registers[src2]) { pc += offset; }

    #define INS_BRANCH_IMM_DO(name, comparitor) \
        uint8_t src1 = getNext(pc + PRISM_INS_##name##_SRC_OFFSET, program); \
        uint64_t imm = getNext8(pc + PRISM_INS_##name##_IMM_OFFSET, program); \
        int64_t offset = getNext8(pc + PRISM_INS_##name##_OFFSET_OFFSET, program); \
        pc += PRISM_INS_##name##_WIDTH; \
        if (current_cf->registers[src1] comparitor imm) { pc += offset; }

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

    #define NEAREST_8(val) ((val + 7) >> 3)

    do_alloc: {
        CHECK_PROGRAM(ALLOC);

        uint64_t imm = NEAREST_8(getNext8(pc + PRISM_INS_ALLOC_IMM_OFFSET, program));

        if ((fp + 1) * CFRAME_WIDTH + (sp + imm) > DATA_SLOTS) {
            EXIT_PROGRAM(PRISM_EX_SIG_ERR + PRISM_SIG_SEGV_SOF);
        }

        sp += imm;
        NEXT_INSTRUCTION(ALLOC);
    }

    do_allocr: {
        CHECK_PROGRAM(ALLOCR);

        uint8_t reg = NEAREST_8(getNext(pc + PRISM_INS_ALLOCR_REG_OFFSET, program));
        uint64_t alloc = current_cf->registers[reg];

        if ((fp + 1) * CFRAME_WIDTH + (sp + alloc > DATA_SLOTS)) {
            EXIT_PROGRAM(PRISM_EX_SIG_ERR + PRISM_SIG_SEGV_SOF);
        }

        sp += alloc;
        NEXT_INSTRUCTION(ALLOCR);
    }
 }
