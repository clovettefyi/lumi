/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org.
 */

#include "lumi_vm.h"

int main(int argc, char** argv) {
    LumiVM* vm = lumiCreateVM();
    lumiDestroyVM(vm);
    return 0;
}
