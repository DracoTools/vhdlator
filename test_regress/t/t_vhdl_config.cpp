// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

#include <verilated.h>

#include VM_PREFIX_INCLUDE

#include <cstdio>

int main(int argc, char** argv) {
    VerilatedContext context;
    context.commandArgs(argc, argv);
    VM_PREFIX* const topp = new VM_PREFIX{&context};
    topp->eval();
    const bool ok = topp->o == 1;
    if (!ok) {
        printf("%%Error: output is %d, so the configuration did not choose the architecture\n",
               (int)topp->o);
    }
    topp->final();
    delete topp;
    printf(ok ? "*-* All Finished *-*\n" : "*-* FAILED *-*\n");
    return ok ? 0 : 10;
}
