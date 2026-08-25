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

    for (unsigned trial = 0; trial < 4; ++trial) {
        const unsigned a = 0xB3u ^ (trial * 0x37u);
        const unsigned b = 0x1234u ^ (trial * 0x2c9u);
        topp->a = a;
        topp->b = b;
        topp->eval();

        unsigned wantRa = 0, wantRb = 0, wantPa = 0, wantPb = 0;
        for (int i = 0; i < 8; ++i) {
            if ((a >> i) & 1) wantRa |= 1u << (7 - i);
            wantPa ^= (a >> i) & 1;
        }
        for (int i = 0; i < 16; ++i) {
            if ((b >> i) & 1) wantRb |= 1u << (15 - i);
            wantPb ^= (b >> i) & 1;
        }
        if (topp->ra != wantRa) {
            printf("%%Error: reverse of eight bits is 0x%02x, expected 0x%02x\n",
                   (unsigned)topp->ra, wantRa);
            ++errors;
        }
        if (topp->rb != wantRb) {
            printf("%%Error: reverse of sixteen bits is 0x%04x, expected 0x%04x\n",
                   (unsigned)topp->rb, wantRb);
            ++errors;
        }
        if (topp->pa != wantPa || topp->pb != wantPb) {
            printf("%%Error: parity is wrong\n");
            ++errors;
        }
        if (topp->sa != (a & 0xffu)) {
            printf("%%Error: mask is 0x%02x, expected 0x%02x\n", (unsigned)topp->sa, a & 0xffu);
            ++errors;
        }
        // A result whose length came from the value the call passes
        if (topp->o4 != 0xfu || topp->o6 != 0x3fu) {
            printf("%%Error: ones is 0x%x and 0x%x, expected 0xf and 0x3f\n", (unsigned)topp->o4,
                   (unsigned)topp->o6);
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
