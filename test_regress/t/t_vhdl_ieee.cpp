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
    topp->en = 1;
    topp->clk = 0;
    topp->eval();
    topp->rst = 0;

    unsigned expectAcc = 0;
    for (int cycle = 1; cycle <= 300; ++cycle) {
        expectAcc += (cycle - 1) & 0xff;  // the value c held before this edge
        topp->clk = 1;
        topp->eval();
        topp->clk = 0;
        topp->eval();
        const unsigned want = cycle & 0xff;
        if (topp->q != want) {
            if (errors < 5) {
                printf("%%Error: cycle %d: q is 0x%02x, expected 0x%02x\n", cycle,
                       (unsigned)topp->q, want);
            }
            ++errors;
        }
        if (topp->odd != (want & 1)) {
            if (errors < 5) printf("%%Error: cycle %d: odd bit is wrong\n", cycle);
            ++errors;
        }
        // to_integer gives an INTEGER, so the sum runs well past 255
        if (topp->acc != static_cast<int>(expectAcc)) {
            if (errors < 5) {
                printf("%%Error: cycle %d: acc is %d, expected %d\n", cycle, (int)topp->acc,
                       (int)expectAcc);
            }
            ++errors;
        }
    }

    topp->final();
    delete topp;
    if (errors) {
        printf("*-* FAILED *-*\n");
        return 10;
    }
    printf("*-* All Finished *-*\n");
    return 0;
}
