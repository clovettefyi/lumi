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

LpInstance* lpCreate(uint64_t stack_size, uint8_t* program, uint64_t program_size) {
    LpInstance* vm = nullptr;
    uint8_t* stack = nullptr;
    uint8_t* programp = nullptr;

    vm = malloc(sizeof(LpInstance));
    if (vm == nullptr) {
        goto cleanup;
    }

    stack = malloc(stack_size);
    if (stack == nullptr) {
        goto cleanup;
    }

    programp = malloc(program_size + 1);
    if (programp == nullptr) {
        goto cleanup;
    }

    vm->stack = stack;
    vm->sp = 0;
    vm->bp = 0;
    vm->stack_size = stack_size;

    memcpy(programp, program, program_size * sizeof(uint8_t));
    programp[program_size] = 0;

    vm->program = programp;
    vm->program_size = program_size;
    vm->pc = 0;

    return vm;

    cleanup: {
        if (programp != nullptr) free(programp);
        if (stack != nullptr) free(stack);
        if (vm != nullptr) free(vm);
        return nullptr;
    }
}

void lpDestroy(LpInstance* vm) {
    if (vm == nullptr) {
        return;
    }

    free(vm->stack);
    free(vm->program);
    free(vm);
}

__attribute__((noinline))
uint8_t lpRun(LpInstance* vm) {
    if (vm == nullptr) {
        return LP_EX_SIG_ERR + LP_SIG_VM_ERR;
    }

    if (vm->program == nullptr) {
        return LP_EX_SIG_ERR + LP_SIG_PROG_ERR;
    }

    const uint8_t* program = vm->program;
    const uint64_t program_size = vm->program_size;
    uint64_t pc = vm->pc;

    uint64_t* registers = vm->registers;

    uint8_t* stack = vm->stack;
    uint64_t sp = vm->sp;
    uint64_t bp = vm->bp;

    const uint64_t stack_size = vm->stack_size;

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

        [LP_INS_FS] = &&do_fs,
        [LP_INS_FSR] = &&do_fsr,
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
        vm->pc = pc; \
        vm->sp = sp; \
        vm->bp = bp; \
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
        EXIT_PROGRAM(registers[0]);
    }

    #define INS_JAL_STACK 16
    #define INS_JAL(new_addr, old_addr) \
        memcpy(stack + sp, &old_addr, sizeof(old_addr)); \
        sp += sizeof(old_addr); \
        memcpy(stack + sp, &bp, sizeof(bp)); \
        sp += sizeof(bp); \
        pc = new_addr; \
        bp = sp; \


    do_jal: {
        CHECK_PROGRAM(JAL);

        if (sp + INS_JAL_STACK > stack_size) {
            EXIT_PROGRAM(LP_EX_SIG_ERR + LP_SIG_SEGV_SOF);
        }

        uint64_t new_addr = getNext8(pc + LP_INS_JAL_ADDR_OFFSET, program);
        uint64_t old_addr = pc + LP_INS_JAL_WIDTH;

        INS_JAL(new_addr, old_addr);

        NEXT_INSTRUCTION_DIRECT();
    }

    do_jalr: {
        CHECK_PROGRAM(JALR);

        if (sp + INS_JAL_STACK > stack_size) {
            EXIT_PROGRAM(LP_EX_SIG_ERR + LP_SIG_SEGV_SOF);
        }

        uint8_t reg = getNext(pc + LP_INS_JALR_REG_OFFSET, program);
        uint64_t new_addr = registers[reg];
        uint64_t old_addr = pc + LP_INS_JALR_WIDTH;

        INS_JAL(new_addr, old_addr);

        NEXT_INSTRUCTION_DIRECT();
    }

    do_ret: {
        if (bp == 0) {
            EXIT_PROGRAM(LP_EX_SIG_ERR + LP_SIG_SEGV_SUF);
        }

        CHECK_PROGRAM(RET);

        uint64_t old_bp, old_addr;
        memcpy(&old_bp, stack + bp - sizeof(old_bp), sizeof(old_bp));
        bp -= sizeof(old_bp);
        memcpy(&old_addr, stack + bp - sizeof(old_addr), sizeof(old_addr));

        bp = old_bp;
        sp = bp;

        pc = old_addr;

        NEXT_INSTRUCTION_DIRECT();
    }

    do_mov: {
        CHECK_PROGRAM(MV);

        uint8_t dst = getNext(pc + LP_INS_MV_DST_OFFSET, program);
        uint8_t src = getNext(pc + LP_INS_MV_SRC_OFFSET, program);

        registers[dst] = registers[src];

        NEXT_INSTRUCTION(MV);
    }

    do_ldi: {
        CHECK_PROGRAM(LDI);

        uint8_t dst = getNext(pc + LP_INS_LDI_DST_OFFSET, program);
        uint64_t imm = getNext8(pc + LP_INS_LDI_IMM_OFFSET, program);

        registers[dst] = imm;

        NEXT_INSTRUCTION(LDI);
    }

    #define INS_ARITH_REG_DO(name, op) \
        uint8_t dst = getNext(pc + LP_INS_##name##_DST_OFFSET, program); \
        uint8_t src1 = getNext(pc + LP_INS_##name##_SRC1_OFFSET, program); \
        uint8_t src2 = getNext(pc + LP_INS_##name##_SRC2_OFFSET, program); \
        registers[dst] = registers[src1] op registers[src2]; \

    #define INS_ARITH_IMM_DO(name, op) \
        uint8_t dst = getNext(pc + LP_INS_##name##_DST_OFFSET, program); \
        uint8_t src = getNext(pc + LP_INS_##name##_SRC_OFFSET, program); \
        uint64_t imm = getNext8(pc + LP_INS_##name##_IMM_OFFSET, program); \
        registers[dst] = registers[src] op imm; \

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
        if (registers[src1] comparitor registers[src2]) { \
            pc += LP_INS_##name##_WIDTH + offset; \
            NEXT_INSTRUCTION_DIRECT(); \
        } \
        pc += LP_INS_##name##_WIDTH; \
        NEXT_INSTRUCTION_DIRECT();

    #define INS_BRANCH_IMM_DO(name, comparitor) \
        uint8_t src = getNext(pc + LP_INS_##name##_SRC_OFFSET, program); \
        uint64_t imm = getNext8(pc + LP_INS_##name##_IMM_OFFSET, program); \
        int64_t offset = getNext8(pc + LP_INS_##name##_OFFSET_OFFSET, program); \
        if (registers[src] comparitor imm) { \
            pc += LP_INS_##name##_WIDTH + offset; \
            NEXT_INSTRUCTION_DIRECT(); \
        } \
        pc += LP_INS_##name##_WIDTH; \
        NEXT_INSTRUCTION_DIRECT();

    do_beq: {
        CHECK_PROGRAM(BEQ);
        INS_BRANCH_REG_DO(BEQ, ==);
    }

    do_beqi: {
        CHECK_PROGRAM(BEQI);
        INS_BRANCH_IMM_DO(BEQI, ==);
    }

    do_bne: {
        CHECK_PROGRAM(BNE);
        INS_BRANCH_REG_DO(BNE, !=);
    }

    do_bnei: {
        CHECK_PROGRAM(BNEI);
        INS_BRANCH_IMM_DO(BNEI, !=);
    }

    do_bgt: {
        CHECK_PROGRAM(BGT);
        INS_BRANCH_REG_DO(BGT, >);
    }

    do_bgti: {
        CHECK_PROGRAM(BGTI);
        INS_BRANCH_IMM_DO(BGTI, >);
    }

    do_blt: {
        CHECK_PROGRAM(BLT);
        INS_BRANCH_REG_DO(BLT, <);
    }

    do_blti: {
        CHECK_PROGRAM(BLTI);
        INS_BRANCH_IMM_DO(BLTI, <);
    }

    do_bge: {
        CHECK_PROGRAM(BGE);
        INS_BRANCH_REG_DO(BGE, >=);
    }

    do_bgei: {
        CHECK_PROGRAM(BGEI);
        INS_BRANCH_IMM_DO(BGEI, >=);
    }

    do_ble: {
        CHECK_PROGRAM(BLE);
        INS_BRANCH_REG_DO(BLE, <=);
    }

    do_blei: {
        CHECK_PROGRAM(BLEI);
        INS_BRANCH_IMM_DO(BLEI, <=);
    }

    do_as: {
        CHECK_PROGRAM(AS);

        uint64_t imm = getNext8(pc + LP_INS_AS_IMM_OFFSET, program);

        if (sp + imm > stack_size) {
            EXIT_PROGRAM(LP_EX_SIG_ERR + LP_SIG_SEGV_SOF);
        }

        sp += imm;
        NEXT_INSTRUCTION(AS);
    }

    do_asr: {
        CHECK_PROGRAM(ASR);

        uint8_t reg = getNext(pc + LP_INS_ASR_REG_OFFSET, program);
        uint64_t alloc = registers[reg];

        if (sp + alloc > stack_size) {
            EXIT_PROGRAM(LP_EX_SIG_ERR + LP_SIG_SEGV_SOF);
        }

        sp += alloc;
        NEXT_INSTRUCTION(ASR);
    }

    #define INS_FREE(free) \
        if (free > sp - bp) { \
            EXIT_PROGRAM(LP_EX_SIG_ERR + LP_SIG_SEGV_SUF); \
        } \
        sp -= free;

    do_fs: {
        CHECK_PROGRAM(FS);

        uint64_t free = getNext8(pc + LP_INS_FS_IMM_OFFSET, program);
        INS_FREE(free);

        NEXT_INSTRUCTION(FS);
    }

    do_fsr: {
        CHECK_PROGRAM(FSR);

        uint64_t reg = getNext(pc + LP_INS_FSR_REG_OFFSET, program);
        uint64_t free = registers[reg];
        INS_FREE(free);

        NEXT_INSTRUCTION(FSR);
    }

 }
