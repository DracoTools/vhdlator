// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0
//
// C calling into VHDL. The exported subprograms arrive as ordinary DPI
// exports, so the C side is the same code a SystemVerilog export would need,
// including svSetScope to say which instance to run in.

#include <verilated.h>

#include "svdpi.h"

#include VM_PREFIX_INCLUDE
#include VM_PREFIX_ROOT_INCLUDE

#include <cstdio>

// The generated header, whose name follows the model prefix
#include "Vt_vhdl_export__Dpi.h"

static int errors = 0;

#define CHECK(cond, what) \
    do { \
        if (!(cond)) { \
            printf("%%Error: %s:%d: %s\n", __FILE__, __LINE__, what); \
            ++errors; \
        } \
    } while (0)

int main(int argc, char** argv) {
    VerilatedContext context;
    context.commandArgs(argc, argv);
    VM_PREFIX* const topp = new VM_PREFIX{&context};

    topp->clk = 0;
    topp->eval();

    // An exported subprogram runs in one instance, named as VPI names it
    const svScope scope = svGetScopeFromName("TOP.t_vhdl_export");
    CHECK(scope, "cannot find the design's scope");
    svSetScope(scope);

    CHECK(vhdl_square(7) == 49, "vhdl_square(7)");
    CHECK(vhdl_square(-3) == 9, "vhdl_square(-3)");

    int lo = 0;
    int hi = 0;
    vhdl_minmax(9, 4, &lo, &hi);
    CHECK(lo == 4, "vhdl_minmax low");
    CHECK(hi == 9, "vhdl_minmax high");

    // The design still runs normally alongside
    for (int cycle = 0; cycle < 3; ++cycle) {
        topp->clk = 1;
        topp->eval();
        topp->clk = 0;
        topp->eval();
    }
    CHECK(topp->q == 3, "the design did not count");

    topp->final();
    delete topp;
    if (errors) {
        printf("%%Error: %d failures\n", errors);
        return 10;
    }
    printf("*-* All Finished *-*\n");
    return 0;
}
