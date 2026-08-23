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
    int errors = 0;

    topp->rst = 1;
    topp->clk = 0;
    topp->eval();
    topp->rst = 0;

    unsigned x = 0;
    unsigned y = 0;
    for (int cycle = 1; cycle <= 40; ++cycle) {
        topp->clk = 1;
        topp->eval();
        topp->clk = 0;
        topp->eval();
        x = (x + 1) & 0xff;
        y = (y + 3) & 0xff;
        const unsigned want = (x + y) & 0xff;
        if (topp->sum != want) {
            if (errors < 5) {
                printf("%%Error: cycle %d: sum is 0x%02x, expected 0x%02x\n", cycle,
                       (unsigned)topp->sum, want);
            }
            ++errors;
        }
    }

    topp->final();
    delete topp;
    printf(errors ? "*-* FAILED *-*\n" : "*-* All Finished *-*\n");
    return errors ? 10 : 0;
}
