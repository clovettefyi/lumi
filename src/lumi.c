/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org.
 */

#include "lumi_vm.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Please specify bytecode file path\n");
        return 1;
    }

    const char* file_path = argv[1];

    FILE* file = fopen(file_path, "rb");
    if (file == nullptr) {
        printf("Failed to open file: %s\n", file_path);
        return 1;
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0) {
        printf("Empty or invalid file\n");
        fclose(file);
        return 1;
    }

    uint8_t* program = malloc(file_size);
    if (program == nullptr) {
        printf("Failed to allocate memory for program buffer!\n");
        fclose(file);
        return 1;
    }

    size_t bytes_read = fread(program, 1, file_size, file);
    fclose(file);

    if (bytes_read != file_size) {
        printf("Failed to read entire file\n");
        free(program);
        return 1;
    }

    LumiVM* vm = lumiCreateVM();
    if (vm == nullptr) {
        printf("Failed to create VM\n");
        free(program);
        return 1;
    }

    int32_t exit_code = lumiRunVM(vm, program, bytes_read);

    printf("Exited with %" PRId32 "\nProgram Counter: 0x%016" PRIx64 "\n", exit_code, vm->pc);

    lumiDestroyVM(vm);
    free(program);

    return 0;
}
