// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

#include <verilated.h>

#include VM_PREFIX_INCLUDE

#include <cstdio>

int errors = 0;

void check(const char* what, int cycle, unsigned got, unsigned expect) {
    if (got != expect) {
        if (errors < 10) {
            printf("%%Error: cycle %d: %s is 0x%02x, expected 0x%02x\n", cycle, what, got,
                   expect);
        }
        ++errors;
    }
}

int main(int argc, char** argv) {
    VerilatedContext context;
    context.commandArgs(argc, argv);
    VM_PREFIX* const topp = new VM_PREFIX{&context};

    // Reset, then release it
    topp->rst = 1;
    topp->en = 1;
    topp->sel = 0;
    topp->clk = 0;
    topp->eval();
    if (topp->count != 0) {
        printf("%%Error: reset did not clear the counter\n");
        ++errors;
    }
    topp->rst = 0;

    // A model of what the design must do
    enum State : uint8_t { IDLE = 0, RUNNING = 1, DONE = 2 };
    unsigned c = 0;
    unsigned sh = 0x01;
    unsigned pipe[3] = {0, 0, 0};
    unsigned state = IDLE;

    for (int cycle = 1; cycle <= 40; ++cycle) {
        topp->sel = cycle & 3;

        // What the registers become on this edge
        const unsigned prevC = c;
        const unsigned prevPipe0 = pipe[0];
        const unsigned prevPipe1 = pipe[1];
        c ^= 0x01;
        sh = ((sh << 1) | (sh >> 7)) & 0xff;
        pipe[0] = prevC;
        pipe[1] = prevPipe0;
        pipe[2] = prevPipe1;
        state = (state == IDLE) ? RUNNING : (state == RUNNING ? DONE : IDLE);

        topp->clk = 1;
        topp->eval();
        topp->clk = 0;
        topp->eval();

        check("count", cycle, topp->count, c);
        // The adder is combinational, so it follows the new register values
        check("adder output", cycle, topp->summed, pipe[2] ^ sh);
        check("conditional assignment", cycle, topp->shift, state == RUNNING ? sh : 0);
        unsigned wantMux = 0;
        switch (cycle & 3) {
        case 0: wantMux = c; break;
        case 1: wantMux = sh; break;
        case 2: wantMux = pipe[2]; break;
        default: wantMux = 0x01; break;
        }
        check("selected assignment", cycle, topp->muxed, wantMux);
        unsigned p = 0;
        for (int b = 0; b < 8; ++b) p ^= (c >> b) & 1;
        check("parity function", cycle, topp->pari, p);
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
