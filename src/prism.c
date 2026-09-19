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

constexpr static size_t GSTACK_SIZE = 1024 * 1024;
constexpr static uint64_t DATA_SLOTS = GSTACK_SIZE / sizeof(uint64_t);
constexpr static uint64_t CFRAME_WIDTH = sizeof(LpCFrame) / sizeof(uint64_t);

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

LpInstance* lpCreate(uint8_t* program, uint64_t program_size) {
    LpInstance* vm = nullptr;
    uint64_t* gstack = nullptr;
    uint8_t* programp = nullptr;

    vm = malloc(sizeof(LpInstance));
    if (vm == nullptr) {
        goto cleanup;
    }

    gstack = malloc(GSTACK_SIZE);
    if (gstack == nullptr) {
        goto cleanup;
    }

    programp = malloc(program_size + 1);
    if (programp == nullptr) {
        goto cleanup;
    }

    vm->gstack = gstack;

    vm->cstack.cframes = (LpCFrame*)gstack;
    vm->cstack.fp = 0;

    vm->dstack.data = gstack + DATA_SLOTS - 1;
    vm->dstack.sp = 0;
    vm->dstack.bp = 0;

    memcpy(programp, program, program_size * sizeof(uint8_t));
    programp[program_size] = 0;

    vm->program = programp;
    vm->program_size = program_size;
    vm->pc = 0;

    return vm;

    cleanup: {
        if (programp != nullptr) free(programp);
        if (gstack != nullptr) free(gstack);
        if (vm != nullptr) free(vm);
        return nullptr;
    }
}

/*
 -- DESTROY VM --
*/

void lpDestroy(LpInstance* vm) {
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
uint8_t lpRun(LpInstance* vm) {
    if (vm == nullptr) {
        return LP_EX_SIG_ERR + LP_SIG_VM_ERR;
    }

    if (vm->program == nullptr) {
        return LP_EX_SIG_ERR + LP_SIG_PROG_ERR;
    }

    // issue: some hoisted variables may cause performance issue
    // worth a review at a later date
    const uint8_t* program = vm->program;
    const uint64_t program_size = vm->program_size;
    uint64_t pc = vm->pc;

    uint64_t fp = vm->cstack.fp;
    LpCFrame* current_cf = vm->cstack.cframes + fp;

    uint64_t* dstack = vm->dstack.data;
    uint64_t sp = vm->dstack.sp;
    uint64_t bp = vm->dstack.bp;

    static const void* dispatch_table[] = {
        [1 ... UINT8_MAX] = &&do_invalid,
        [0] = &&do_segv_pc,

        [LP_INS_NOP] = &&do_nop,

        [LP_INS_EX] = &&do_ex,

        [LP_INS_JAL] = &&do_jal,
        [LP_INS_JALR] = &&do_jalr,
        [LP_INS_RET] = &&do_ret,

        [LP_INS_MOV] = &&do_mov,
        [LP_INS_LDI] = &&do_ldi,

        [LP_INS_ADD] = &&do_add,
        [LP_INS_ADDI] = &&do_addi,

        [LP_INS_SUB] = &&do_sub,
        [LP_INS_SUBI] = &&do_subi,

        [LP_INS_MUL] = &&do_mul,
        [LP_INS_MULI] = &&do_muli,

        [LP_INS_JMP] = &&do_jmp,

        [LP_INS_BEQ] = &&do_beq,
        [LP_INS_BEQI] = &&do_beqi,

        [LP_INS_BNE] = &&do_bne,
        [LP_INS_BNEI] = &&do_bnei,

        [LP_INS_BGT] = &&do_bgt,
        [LP_INS_BGTI] = &&do_bgti,

        [LP_INS_BLT] = &&do_blt,
        [LP_INS_BLTI] = &&do_blti,

        [LP_INS_BGE] = &&do_bge,
        [LP_INS_BGEI] = &&do_bgei,

        [LP_INS_BLE] = &&do_ble,
        [LP_INS_BLEI] = &&do_blei,

        [LP_INS_AS] = &&do_as,
        [LP_INS_ASR] = &&do_asr,
    };

    goto *dispatch_table[program[pc]];

    #define CHECK_PROGRAM(name) \
        if (!hasNext(pc, program_size, LP_INS_##name##_WIDTH)) { \
            EXIT_PROGRAM(LP_EX_SIG_ERR + LP_SIG_SEGV_PC); \
        }

    #define NEXT_INSTRUCTION_DIRECT() goto *dispatch_table[program[pc]];

    #define NEXT_INSTRUCTION(name) \
        pc += LP_INS_##name##_WIDTH; \
        goto *dispatch_table[program[pc]];

    #define EXIT_PROGRAM(exit_code) \
        vm->cstack.fp = fp; \
        vm->dstack.sp = sp; \
        vm->dstack.bp = bp; \
        vm->pc = pc; \
        return exit_code;

    /*
     -- INSTRUCTIONS --
    */

    do_segv_pc: {
        EXIT_PROGRAM(LP_EX_SIG_ERR + LP_SIG_SEGV_PC);
    }

    do_invalid: {
        EXIT_PROGRAM(LP_EX_SIG_ERR + LP_SIG_ILL);
    }

    do_nop: {
        NEXT_INSTRUCTION(NOP);
    }

    do_ex: {
        pc += LP_INS_EX_WIDTH;
        EXIT_PROGRAM(current_cf->registers[0]);
    }

    do_jal: {
        CHECK_PROGRAM(JAL);

        if ((fp + 2) * CFRAME_WIDTH + (sp + 1) > DATA_SLOTS) {
            EXIT_PROGRAM(LP_EX_SIG_ERR + LP_SIG_SEGV_SOF);
        }

        uint8_t start_reg = getNext(pc + LP_INS_JAL_SREG_OFFSET, program);
        uint8_t end_reg = getNext(pc + LP_INS_JAL_EREG_OFFSET, program);

        if (start_reg > end_reg) {
            EXIT_PROGRAM(LP_EX_SIG_ERR + LP_SIG_ILL);
        }

        uint64_t jmp_pc = getNext8(pc + LP_INS_JAL_PC_OFFSET, program);
        uint64_t ret_pc = pc + LP_INS_JAL_WIDTH;

        LpCFrame* caller_cf = current_cf;
        fp++;
        current_cf++;
        LpCFrame* callee_cf = current_cf;

        callee_cf->pc = ret_pc;

        size_t reg_count = end_reg - start_reg + 1;

        uint64_t* out_regs = caller_cf->registers + start_reg;
        uint64_t* in_regs = callee_cf->registers;

        memcpy(in_regs, out_regs, reg_count * sizeof(uint64_t));

        pc = jmp_pc;

        *(dstack - sp++) = bp;
        bp = sp;

        NEXT_INSTRUCTION_DIRECT();
    }

    do_jalr: {
        CHECK_PROGRAM(JALR);

        if ((fp + 2) * CFRAME_WIDTH + (sp + 1) > DATA_SLOTS) {
            EXIT_PROGRAM(LP_EX_SIG_ERR + LP_SIG_SEGV_SOF);
        }

        uint8_t start_reg = getNext(pc + LP_INS_JALR_SREG_OFFSET, program);
        uint8_t end_reg = getNext(pc + LP_INS_JALR_EREG_OFFSET, program);

        if (start_reg > end_reg) {
            EXIT_PROGRAM(LP_EX_SIG_ERR + LP_SIG_ILL);
        }

        uint8_t reg = getNext(pc + LP_INS_JALR_REG_OFFSET, vm->program);
        uint64_t jmp_pc = current_cf->registers[reg];
        uint64_t ret_pc = pc + LP_INS_JALR_WIDTH;

        LpCFrame* caller_cf = current_cf;
        fp++;
        current_cf++;
        LpCFrame* callee_cf = current_cf;

        callee_cf->pc = ret_pc;

        size_t reg_count = end_reg - start_reg + 1;

        uint64_t* out_regs = caller_cf->registers + start_reg;
        uint64_t* in_regs = callee_cf->registers;

        memcpy(in_regs, out_regs, reg_count * sizeof(uint64_t));

        pc = jmp_pc;

        *(dstack - sp++) = bp;
        bp = sp;

        NEXT_INSTRUCTION_DIRECT();
    }

    do_ret: {
        if (fp == 0) {
            EXIT_PROGRAM(LP_EX_SIG_ERR + LP_SIG_ILL);
        }

        CHECK_PROGRAM(RET);

        uint64_t* ret_reg = current_cf->registers + getNext(pc + LP_INS_RET_REG_OFFSET, program);

        pc = current_cf->pc;
        fp--;
        current_cf--;

        memcpy(current_cf->registers, ret_reg, sizeof(uint64_t));

        sp = bp - 1;
        bp = *(dstack - sp);

        NEXT_INSTRUCTION_DIRECT();
    }

    do_mov: {
        CHECK_PROGRAM(MOV);

        uint8_t dst = getNext(pc + LP_INS_MOV_DST_OFFSET, program);
        uint8_t src = getNext(pc + LP_INS_MOV_SRC_OFFSET, program);
        LpCFrame* cf = current_cf;

        cf->registers[dst] = cf->registers[src];

        NEXT_INSTRUCTION(MOV);
    }

    do_ldi: {
        CHECK_PROGRAM(LDI);

        uint8_t dst = getNext(pc + LP_INS_LDI_DST_OFFSET, program);
        uint64_t imm = getNext8(pc + LP_INS_LDI_IMM_OFFSET, program);
        LpCFrame* cf = current_cf;

        cf->registers[dst] = imm;

        NEXT_INSTRUCTION(LDI);
    }

    #define INS_ARITH_REG_DO(name, op) \
        uint8_t dst = getNext(pc + LP_INS_##name##_DST_OFFSET, program); \
        uint8_t src1 = getNext(pc + LP_INS_##name##_SRC1_OFFSET, program); \
        uint8_t src2 = getNext(pc + LP_INS_##name##_SRC2_OFFSET, program); \
        current_cf->registers[dst] = current_cf->registers[src1] op current_cf->registers[src2]; \

    #define INS_ARITH_IMM_DO(name, op) \
        uint8_t dst = getNext(pc + LP_INS_##name##_DST_OFFSET, program); \
        uint8_t src = getNext(pc + LP_INS_##name##_SRC_OFFSET, program); \
        uint64_t imm = getNext8(pc + LP_INS_##name##_IMM_OFFSET, program); \
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

        uint64_t jmp_pc = getNext8(pc + LP_INS_JMP_PC_OFFSET, program);
        pc = jmp_pc;

        NEXT_INSTRUCTION_DIRECT();
    }

    #define INS_BRANCH_REG_DO(name, comparitor) \
        uint8_t src1 = getNext(pc + LP_INS_##name##_SRC1_OFFSET, program); \
        uint8_t src2 = getNext(pc + LP_INS_##name##_SRC2_OFFSET, program); \
        int64_t offset = getNext8(pc + LP_INS_##name##_OFFSET_OFFSET, program); \
        pc += LP_INS_##name##_WIDTH; \
        if (current_cf->registers[src1] comparitor current_cf->registers[src2]) { pc += offset; }

    #define INS_BRANCH_IMM_DO(name, comparitor) \
        uint8_t src1 = getNext(pc + LP_INS_##name##_SRC_OFFSET, program); \
        uint64_t imm = getNext8(pc + LP_INS_##name##_IMM_OFFSET, program); \
        int64_t offset = getNext8(pc + LP_INS_##name##_OFFSET_OFFSET, program); \
        pc += LP_INS_##name##_WIDTH; \
        if (current_cf->registers[src1] comparitor imm) { pc += offset; }

    do_beq: {
        CHECK_PROGRAM(BEQ);
        INS_BRANCH_REG_DO(BEQ, ==);
        NEXT_INSTRUCTION_DIRECT()
    }

    do_beqi: {
        CHECK_PROGRAM(BEQI);
        INS_BRANCH_IMM_DO(BEQI, ==);
        NEXT_INSTRUCTION_DIRECT()
    }

    do_bne: {
        CHECK_PROGRAM(BNE);
        INS_BRANCH_REG_DO(BNE, !=);
        NEXT_INSTRUCTION_DIRECT();
    }

    do_bnei: {
        CHECK_PROGRAM(BNEI);
        INS_BRANCH_IMM_DO(BNEI, !=);
        NEXT_INSTRUCTION_DIRECT();
    }

    do_bgt: {
        CHECK_PROGRAM(BGT);
        INS_BRANCH_REG_DO(BGT, >);
        NEXT_INSTRUCTION_DIRECT();
    }

    do_bgti: {
        CHECK_PROGRAM(BGTI);
        INS_BRANCH_IMM_DO(BGTI, >);
        NEXT_INSTRUCTION_DIRECT();
    }

    do_blt: {
        CHECK_PROGRAM(BLT);
        INS_BRANCH_REG_DO(BLT, <);
        NEXT_INSTRUCTION_DIRECT();
    }

    do_blti: {
        CHECK_PROGRAM(BLTI);
        INS_BRANCH_IMM_DO(BLTI, <);
        NEXT_INSTRUCTION_DIRECT();
    }

    do_bge: {
        CHECK_PROGRAM(BGE);
        INS_BRANCH_REG_DO(BGE, >=);
        NEXT_INSTRUCTION_DIRECT();
    }

    do_bgei: {
        CHECK_PROGRAM(BGEI);
        INS_BRANCH_IMM_DO(BGEI, >=);
        NEXT_INSTRUCTION_DIRECT();
    }

    do_ble: {
        CHECK_PROGRAM(BLE);
        INS_BRANCH_REG_DO(BLE, <=);
        NEXT_INSTRUCTION_DIRECT();
    }

    do_blei: {
        CHECK_PROGRAM(BLEI);
        INS_BRANCH_IMM_DO(BLEI, <=);
        NEXT_INSTRUCTION_DIRECT();
    }

    #define NEAREST_8(val) ((val + 7) >> 3)

    do_as: {
        CHECK_PROGRAM(AS);

        uint64_t imm = NEAREST_8(getNext8(pc + LP_INS_AS_IMM_OFFSET, program));

        if (((fp + 1) * CFRAME_WIDTH + (sp + imm)) > DATA_SLOTS) {
            EXIT_PROGRAM(LP_EX_SIG_ERR + LP_SIG_SEGV_SOF);
        }

        sp += imm;
        NEXT_INSTRUCTION(AS);
    }

    do_asr: {
        CHECK_PROGRAM(ASR);

        uint8_t reg = NEAREST_8(getNext(pc + LP_INS_ASR_REG_OFFSET, program));
        uint64_t alloc = current_cf->registers[reg];

        if (((fp + 1) * CFRAME_WIDTH + (sp + alloc)) > DATA_SLOTS) {
            EXIT_PROGRAM(LP_EX_SIG_ERR + LP_SIG_SEGV_SOF);
        }

        sp += alloc;
        NEXT_INSTRUCTION(ASR);
    }
 }
