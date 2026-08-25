// DESCRIPTION: Verilator Example: reading and writing VHDL objects over VPI
//
// This file ONLY is placed under the Creative Commons Public Domain
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0
//
// A VHDL object lands in the same tables a Verilog one does, so the standard
// VPI calls reach it with nothing language-specific involved. This is also
// how cocotb reaches a VHDL design: over VPI, not VHPI.
//
// VHDL is not case sensitive and folds a basic identifier to lower case; that
// is the spelling VPI sees, and the one to use in a name.

#include <verilated.h>
#include <verilated_vpi.h>

#include "Vtop.h"

#include <cstdio>

int main(int argc, char** argv) {
    VerilatedContext context;
    context.commandArgs(argc, argv);
    Vtop* const topp = new Vtop{&context};

    vpiHandle countp = vpi_handle_by_name(const_cast<char*>("TOP.top.u_cnt.count"), nullptr);
    vpiHandle widthp = vpi_handle_by_name(const_cast<char*>("TOP.top.u_cnt.width"), nullptr);
    if (!countp || !widthp) {
        printf("%%Error: could not find the VHDL objects over VPI\n");
        return 10;
    }

    // A generic reads back the way a parameter does
    s_vpi_value val;
    val.format = vpiIntVal;
    vpi_get_value(widthp, &val);
    printf("generic WIDTH = %d, count is %d bits wide\n", val.value.integer,
           vpi_get(vpiSize, countp));

    topp->rst = 1;
    for (int i = 0; i < 4; ++i) {
        topp->clk = !topp->clk;
        context.timeInc(1);
        topp->eval();
    }
    topp->rst = 0;

    for (int cycle = 0; cycle < 10; ++cycle) {
        topp->clk = 1;
        context.timeInc(1);
        topp->eval();
        topp->clk = 0;
        context.timeInc(1);
        topp->eval();
    }

    vpi_get_value(countp, &val);
    printf("count after ten cycles = %d\n", val.value.integer);
    if (val.value.integer != 10) {
        printf("%%Error: expected 10\n");
        return 10;
    }

    // Writing works too; the value takes effect at the next eval()
    val.value.integer = 0x42;
    vpi_put_value(countp, &val, nullptr, vpiNoDelay);
    topp->eval();
    vpi_get_value(countp, &val);
    printf("count after being written = 0x%02x\n", val.value.integer);

    topp->final();
    delete topp;
    printf("-- VPI reached the VHDL. --\n");
    return 0;
}
