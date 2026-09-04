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

constexpr static size_t CFRAME_REGISTERS = 256;
constexpr static size_t CFRAME_COUNT = 128;
constexpr static size_t DSTACK_BYTE_COUNT = 1024 * 1024;

typedef struct {
    uint64_t registers[CFRAME_REGISTERS];
    uint64_t fp;
} CFrame;

struct LumiVM {
    struct {
        CFrame* cframes;
        uint64_t accumulator;
    } cstack;

    struct {
        uint8_t* data;
        uint64_t sp;
        uint64_t bp;
    } dstack;

    const uint8_t* program;
    size_t program_length;
    uint64_t pc;
};

static inline uint8_t getNext8(LumiVM* vm) {
    return vm->program[vm->pc];
}

static inline uint16_t getNext16(LumiVM* vm) {
    uint8_t byte0 = vm->program[vm->pc];
    uint8_t byte1 = vm->program[vm->pc + 1];

    return ((uint16_t)byte1 << 8) | byte0;
}

static inline uint32_t getNext32(LumiVM* vm) {
    uint8_t byte0 = vm->program[vm->pc];
    uint8_t byte1 = vm->program[vm->pc + 1];
    uint8_t byte2 = vm->program[vm->pc + 2];
    uint8_t byte3 = vm->program[vm->pc + 3];

    return ((uint32_t)byte3 << 24) |
           ((uint32_t)byte2 << 16) |
           ((uint32_t)byte1 << 8)  |
           (uint32_t)byte0;
}

static inline uint64_t getNext64(LumiVM* vm) {
    uint8_t byte0 = vm->program[vm->pc];
    uint8_t byte1 = vm->program[vm->pc + 1];
    uint8_t byte2 = vm->program[vm->pc + 2];
    uint8_t byte3 = vm->program[vm->pc + 3];
    uint8_t byte4 = vm->program[vm->pc + 4];
    uint8_t byte5 = vm->program[vm->pc + 5];
    uint8_t byte6 = vm->program[vm->pc + 6];
    uint8_t byte7 = vm->program[vm->pc + 7];

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

int32_t lumiRunVM(LumiVM* vm, const uint8_t* program) {
    if (vm == nullptr) {
        return EX_STATE_ERR;
    }

    if (program == nullptr) {
        return EX_PROGRAM_ERR;
    }

    vm->program = program;

    return EX_OKAY;
}
