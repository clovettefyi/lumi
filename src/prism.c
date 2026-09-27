/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org.
 */

#include "lprism.h"

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

#define PROGRAM_VALIDATION() \
    if (program == nullptr || \
        program_size == 0 || \
        program_size == UINT64_MAX) { \
        return nullptr; \
    }


#define CLEAN_UP(vm, vm_program, vm_stack) \
    if (vm != nullptr) { \
        if (vm_program != nullptr) free(vm_program); \
        if (vm_stack != nullptr) free(vm_stack); \
        free(vm); \
    }


LpInstance* lpCreate(uint64_t stack_size, uint8_t* program, uint64_t program_size) {
    PROGRAM_VALIDATION();

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
    vm->stack_size = stack_size;

    memset(vm->registers, 0, LP_REG_COUNT * sizeof(*vm->registers));
    memset(vm->stack, 0, vm->stack_size * sizeof(*vm->stack));

    memcpy(programp, program, program_size * sizeof(*program));
    programp[program_size] = 0;

    vm->program = programp;
    vm->program_size = program_size;
    vm->pc = 0;

    return vm;

    cleanup: {
        CLEAN_UP(vm, programp, stack);
        return nullptr;
    }
}

LpInstance* lpReset(LpInstance* vm) {
    if (vm == nullptr) {
        return nullptr;
    }

    if (vm->stack == nullptr ||
        vm->program == nullptr) {
        return nullptr;
    }

    memset(vm->registers, 0, LP_REG_COUNT * sizeof(*vm->registers));
    memset(vm->stack, 0, vm->stack_size * sizeof(*vm->stack));
    vm->pc = 0;

    return vm;
}

LpInstance* lpResetNew(LpInstance* vm, uint8_t* program, uint64_t program_size) {
    PROGRAM_VALIDATION();

    if (lpReset(vm) == nullptr) {
        return nullptr;
    }

    if (vm->program == program && vm->program_size == program_size) {
        return vm;
    }

    uint8_t* programp = malloc(program_size + 1);
    if (programp == nullptr) {
        return nullptr;
    }

    memcpy(programp, program, program_size * sizeof(*program));
    free(vm->program);
    vm->program = programp;
    vm->program[program_size] = 0;
    vm->program_size = program_size;

    return vm;
}

void lpDestroy(LpInstance* vm) {
    CLEAN_UP(vm, vm->program, vm->stack);
}

__attribute__((noinline))
uint8_t lpRun(LpInstance* vm) {
    if (vm == nullptr) {
        return LP_EX_SIG_ERR + LP_SIG_VM_ERR;
    }

    if (vm->program == nullptr ||
        vm->stack == nullptr) {
        return LP_EX_SIG_ERR + LP_SIG_VM_ERR;
    }

    const uint8_t* program = vm->program;
    const uint64_t program_size = vm->program_size;
    uint64_t pc = vm->pc;

    uint64_t* registers = vm->registers;

    uint8_t* stack = vm->stack;

    #define BP registers[LP_BP_REG]
    #define SP registers[LP_SP_REG]

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
        [LP_INS_MOVI] = &&do_movi,

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

        [LP_INS_LD_B] = &&do_ld_b,
        [LP_INS_LD_W] = &&do_ld_w,
        [LP_INS_LD_D] = &&do_ld_d,
        [LP_INS_LD_Q] = &&do_ld_q,

        [LP_INS_LDO_B] = &&do_ldo_b,
        [LP_INS_LDO_W] = &&do_ldo_w,
        [LP_INS_LDO_D] = &&do_ldo_d,
        [LP_INS_LDO_Q] = &&do_ldo_q,

        [LP_INS_LDR_B] = &&do_ldr_b,
        [LP_INS_LDR_W] = &&do_ldr_w,
        [LP_INS_LDR_D] = &&do_ldr_d,
        [LP_INS_LDR_Q] = &&do_ldr_q,

        [LP_INS_ST_B] = &&do_st_b,
        [LP_INS_ST_W] = &&do_st_w,
        [LP_INS_ST_D] = &&do_st_d,
        [LP_INS_ST_Q] = &&do_st_q,

        [LP_INS_STO_B] = &&do_sto_b,
        [LP_INS_STO_W] = &&do_sto_w,
        [LP_INS_STO_D] = &&do_sto_d,
        [LP_INS_STO_Q] = &&do_sto_q,

        [LP_INS_STR_B] = &&do_str_b,
        [LP_INS_STR_W] = &&do_str_w,
        [LP_INS_STR_D] = &&do_str_d,
        [LP_INS_STR_Q] = &&do_str_q,
    };

    #define VM_DISPATCH() \
        goto *dispatch_table[program[pc]];

    #define INS_VALIDATION(name) \
        if (!hasNext(pc, program_size, LP_INS_##name##_WIDTH)) { \
            VM_EXIT(LP_EX_SIG_ERR + LP_SIG_SEGV_PC); \
        }

    #define INS_STEP(name) \
        pc += LP_INS_##name##_WIDTH; \
        VM_DISPATCH();

    #define VM_EXIT(exit_code) \
        vm->pc = pc; \
        return exit_code;

    VM_DISPATCH();

    do_segv_pc: {
        VM_EXIT(LP_EX_SIG_ERR + LP_SIG_SEGV_PC);
    }

    do_invalid: {
        VM_EXIT(LP_EX_SIG_ERR + LP_SIG_ILL);
    }

    do_nop: {
        INS_STEP(NOP);
    }

    do_ex: {
        pc += LP_INS_EX_WIDTH;
        VM_EXIT(registers[0]);
    }

    #define INS_JAL_STACK 16
    #define INS_JAL(new_addr, old_addr) \
        memcpy(stack + SP, &old_addr, sizeof(old_addr)); \
        SP += sizeof(old_addr); \
        memcpy(stack + SP, &BP, sizeof(BP)); \
        SP += sizeof(BP); \
        pc = new_addr; \
        BP = SP; \


    do_jal: {
        INS_VALIDATION(JAL);

        if (SP + INS_JAL_STACK > stack_size) {
            VM_EXIT(LP_EX_SIG_ERR + LP_SIG_SEGV_SOF);
        }

        uint64_t new_addr = getNext8(pc + LP_INS_JAL_ADDR_OFFSET, program);
        uint64_t old_addr = pc + LP_INS_JAL_WIDTH;

        INS_JAL(new_addr, old_addr);

        VM_DISPATCH();
    }

    do_jalr: {
        INS_VALIDATION(JALR);

        if (SP + INS_JAL_STACK > stack_size) {
            VM_EXIT(LP_EX_SIG_ERR + LP_SIG_SEGV_SOF);
        }

        uint8_t reg = getNext(pc + LP_INS_JALR_DST_OFFSET, program);
        uint64_t new_addr = registers[reg];
        uint64_t old_addr = pc + LP_INS_JALR_WIDTH;

        INS_JAL(new_addr, old_addr);

        VM_DISPATCH();
    }

    do_ret: {
        if (BP == 0) {
            VM_EXIT(LP_EX_SIG_ERR + LP_SIG_SEGV_SUF);
        }

        INS_VALIDATION(RET);

        uint64_t old_bp, old_addr;
        memcpy(&old_bp, stack + BP - sizeof(old_bp), sizeof(old_bp));
        BP -= sizeof(old_bp);
        memcpy(&old_addr, stack + BP - sizeof(old_addr), sizeof(old_addr));

        BP = old_bp;
        SP = BP;

        pc = old_addr;

        VM_DISPATCH();
    }

    do_mov: {
        INS_VALIDATION(MOV);

        uint8_t dst = getNext(pc + LP_INS_MOV_DST_OFFSET, program);
        uint8_t src = getNext(pc + LP_INS_MOV_SRC_OFFSET, program);

        registers[dst] = registers[src];

        INS_STEP(MOV);
    }

    do_movi: {
        INS_VALIDATION(MOVI);

        uint8_t dst = getNext(pc + LP_INS_MOVI_DST_OFFSET, program);
        uint64_t imm = getNext8(pc + LP_INS_MOVI_IMM_OFFSET, program);

        registers[dst] = imm;

        INS_STEP(MOVI);
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
        INS_VALIDATION(ADD);
        INS_ARITH_REG_DO(ADD, +);
        INS_STEP(ADD);
    }

    do_addi: {
        INS_VALIDATION(ADDI);
        INS_ARITH_IMM_DO(ADDI, +);
        INS_STEP(ADDI);
    }

    do_sub: {
        INS_VALIDATION(SUB);
        INS_ARITH_REG_DO(SUB, -);
        INS_STEP(SUB);
    }

    do_subi: {
        INS_VALIDATION(SUBI);
        INS_ARITH_IMM_DO(SUBI, -);
        INS_STEP(SUBI);
    }

    do_mul: {
        INS_VALIDATION(MUL);
        INS_ARITH_REG_DO(MUL, *);
        INS_STEP(MUL);
    }

    do_muli: {
        INS_VALIDATION(MULI);
        INS_ARITH_IMM_DO(MULI, *);
        INS_STEP(MULI);
    }

    do_jmp: {
        INS_VALIDATION(JMP);

        uint64_t jmp_pc = getNext8(pc + LP_INS_JMP_ADDR_OFFSET, program);
        pc = jmp_pc;

        VM_DISPATCH();
    }

    #define INS_BRANCH_REG_DO(name, comparitor) \
        uint8_t src1 = getNext(pc + LP_INS_##name##_SRC1_OFFSET, program); \
        uint8_t src2 = getNext(pc + LP_INS_##name##_SRC2_OFFSET, program); \
        int64_t offset = getNext8(pc + LP_INS_##name##_OFFSET_OFFSET, program); \
        if (registers[src1] comparitor registers[src2]) { \
            pc += LP_INS_##name##_WIDTH + offset; \
            VM_DISPATCH(); \
        } \
        pc += LP_INS_##name##_WIDTH; \
        VM_DISPATCH();

    #define INS_BRANCH_IMM_DO(name, comparitor) \
        uint8_t src = getNext(pc + LP_INS_##name##_SRC_OFFSET, program); \
        uint64_t imm = getNext8(pc + LP_INS_##name##_IMM_OFFSET, program); \
        int64_t offset = getNext8(pc + LP_INS_##name##_OFFSET_OFFSET, program); \
        if (registers[src] comparitor imm) { \
            pc += LP_INS_##name##_WIDTH + offset; \
            VM_DISPATCH(); \
        } \
        pc += LP_INS_##name##_WIDTH; \
        VM_DISPATCH();

    do_beq: {
        INS_VALIDATION(BEQ);
        INS_BRANCH_REG_DO(BEQ, ==);
    }

    do_beqi: {
        INS_VALIDATION(BEQI);
        INS_BRANCH_IMM_DO(BEQI, ==);
    }

    do_bne: {
        INS_VALIDATION(BNE);
        INS_BRANCH_REG_DO(BNE, !=);
    }

    do_bnei: {
        INS_VALIDATION(BNEI);
        INS_BRANCH_IMM_DO(BNEI, !=);
    }

    do_bgt: {
        INS_VALIDATION(BGT);
        INS_BRANCH_REG_DO(BGT, >);
    }

    do_bgti: {
        INS_VALIDATION(BGTI);
        INS_BRANCH_IMM_DO(BGTI, >);
    }

    do_blt: {
        INS_VALIDATION(BLT);
        INS_BRANCH_REG_DO(BLT, <);
    }

    do_blti: {
        INS_VALIDATION(BLTI);
        INS_BRANCH_IMM_DO(BLTI, <);
    }

    do_bge: {
        INS_VALIDATION(BGE);
        INS_BRANCH_REG_DO(BGE, >=);
    }

    do_bgei: {
        INS_VALIDATION(BGEI);
        INS_BRANCH_IMM_DO(BGEI, >=);
    }

    do_ble: {
        INS_VALIDATION(BLE);
        INS_BRANCH_REG_DO(BLE, <=);
    }

    do_blei: {
        INS_VALIDATION(BLEI);
        INS_BRANCH_IMM_DO(BLEI, <=);
    }

    #define INS_LD_SET(addr, src, bytes) \
        memcpy(stack + BP + addr, registers + src, bytes * sizeof(uint8_t));

    #define INS_LD_DO(bytes, char) \
        uint8_t src = getNext(pc + LP_INS_LD_##char##_SRC_OFFSET, program); \
        uint64_t addr = getNext8(pc + LP_INS_LD_##char##_ADDR_OFFSET, program); \
        INS_LD_SET(addr, src, bytes);

    do_ld_b: {
        INS_VALIDATION(LD_B);
        INS_LD_DO(1, B);
        INS_STEP(LD_B);
    }

    do_ld_w: {
        INS_VALIDATION(LD_W);
        INS_LD_DO(2, W);
        INS_STEP(LD_W);
    }

    do_ld_d: {
        INS_VALIDATION(LD_D);
        INS_LD_DO(4, D);
        INS_STEP(LD_D);
    }

    do_ld_q: {
        INS_VALIDATION(LD_Q);
        INS_LD_DO(8, Q);
        INS_STEP(LD_Q);
    }

    #define INS_LDO_DO(bytes, char) \
        uint8_t src = getNext(pc + LP_INS_LDO_##char##_SRC_OFFSET, program); \
        uint8_t dst = getNext(pc + LP_INS_LDO_##char##_DST_OFFSET, program); \
        uint64_t addr = registers[dst]; \
        int64_t offset = getNext8(pc + LP_INS_LDO_##char##_OFFSET_OFFSET, program); \
        INS_LD_SET(addr + offset, src, bytes)

    do_ldo_b: {
        INS_VALIDATION(LDO_B);
        INS_LDO_DO(1, B);
        INS_STEP(LDO_B);
    }

    do_ldo_w: {
        INS_VALIDATION(LDO_W);
        INS_LDO_DO(2, W);
        INS_STEP(LDO_W);
    }

    do_ldo_d: {
        INS_VALIDATION(LDO_D);
        INS_LDO_DO(4, D);
        INS_STEP(LDO_D);
    }

    do_ldo_q: {
        INS_VALIDATION(LDO_Q);
        INS_LDO_DO(8, Q);
        INS_STEP(LDO_Q);
    }

    #define INS_LDR_DO(bytes, char) \
        uint8_t src = getNext(pc + LP_INS_LDR_##char##_SRC_OFFSET, program); \
        uint8_t dst = getNext(pc + LP_INS_LDR_##char##_DST_OFFSET, program); \
        uint8_t offset_reg = getNext(pc + LP_INS_LDR_##char##_OFFSET_REG_OFFSET, program); \
        uint64_t addr = registers[dst]; \
        int64_t offset = registers[offset_reg]; \
        INS_LD_SET(addr + offset, src, bytes);

    do_ldr_b: {
        INS_VALIDATION(LDR_B);
        INS_LDR_DO(1, B);
        INS_STEP(LDR_B);
    }

    do_ldr_w: {
        INS_VALIDATION(LDR_W);
        INS_LDR_DO(2, W);
        INS_STEP(LDR_W);
    }

    do_ldr_d: {
        INS_VALIDATION(LDR_D);
        INS_LDR_DO(4, D);
        INS_STEP(LDR_D);
    }

    do_ldr_q: {
        INS_VALIDATION(LDR_Q);
        INS_LDR_DO(8, Q);
        INS_STEP(LDR_Q);
    }

    #define INS_ST_SET(dst, addr, bytes) \
        memcpy(registers + dst, stack + BP + addr, bytes * sizeof(uint8_t)); \
        memset((uint8_t*)(registers + dst) + bytes * sizeof(uint8_t), 0, sizeof(uint64_t) - bytes * sizeof(uint8_t));

    #define INS_ST_DO(bytes, char) \
        uint8_t dst = getNext(pc + LP_INS_ST_##char##_DST_OFFSET, program); \
        uint64_t addr = getNext8(pc + LP_INS_ST_##char##_ADDR_OFFSET, program); \
        INS_ST_SET(dst, addr, bytes);

    do_st_b: {
        INS_VALIDATION(ST_B);
        INS_ST_DO(1, B);
        INS_STEP(ST_B);
    }

    do_st_w: {
        INS_VALIDATION(ST_W);
        INS_ST_DO(2, W);
        INS_STEP(ST_W);
    }

    do_st_d: {
        INS_VALIDATION(ST_D);
        INS_ST_DO(4, D);
        INS_STEP(ST_D);
    }

    do_st_q: {
        INS_VALIDATION(ST_Q);
        INS_ST_DO(8, Q);
        INS_STEP(ST_Q);
    }

    #define INS_STO_DO(bytes, char) \
        uint8_t dst = getNext(pc + LP_INS_STO_##char##_DST_OFFSET, program); \
        uint8_t src = getNext(pc + LP_INS_STO_##char##_SRC_OFFSET, program); \
        int64_t offset = getNext8(pc + LP_INS_STO_##char##_OFFSET_OFFSET, program); \
        uint64_t addr = registers[src]; \
        INS_ST_SET(dst, addr + offset, bytes);

    do_sto_b: {
        INS_VALIDATION(STO_B);
        INS_STO_DO(1, B);
        INS_STEP(STO_B);
    }

    do_sto_w: {
        INS_VALIDATION(STO_W);
        INS_STO_DO(2, W);
        INS_STEP(STO_W);
    }

    do_sto_d: {
        INS_VALIDATION(STO_D);
        INS_STO_DO(4, D);
        INS_STEP(STO_D);
    }

    do_sto_q: {
        INS_VALIDATION(STO_Q);
        INS_STO_DO(8, Q);
        INS_STEP(STO_Q);
    }

    #define INS_STR_DO(bytes, char) \
        uint8_t dst = getNext(pc + LP_INS_STR_##char##_DST_OFFSET, program); \
        uint8_t src = getNext(pc + LP_INS_STR_##char##_SRC_OFFSET, program); \
        uint8_t offset_reg = getNext(pc + LP_INS_STR_##char##_OFFSET_REG_OFFSET, program); \
        uint64_t addr = registers[src]; \
        int64_t offset = registers[offset_reg]; \
        INS_ST_SET(dst, addr + offset, bytes);

    do_str_b: {
        INS_VALIDATION(STR_B);
        INS_STR_DO(1, B);
        INS_STEP(STR_B);
    }

    do_str_w: {
        INS_VALIDATION(STR_W);
        INS_STR_DO(2, W);
        INS_STEP(STR_W);
    }

    do_str_d: {
        INS_VALIDATION(STR_D);
        INS_STR_DO(4, D);
        INS_STEP(STR_D);
    }

    do_str_q: {
        INS_VALIDATION(STR_Q);
        INS_STR_DO(8, Q);
        INS_STEP(STR_Q);
    }
 }
