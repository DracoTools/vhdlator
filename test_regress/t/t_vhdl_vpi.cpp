// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0
//
// VPI against a VHDL design. A VHDL object lands in the same
// VerilatedScope/VerilatedVar tables a Verilog one does, so the standard VPI
// calls reach it with nothing language-specific involved. VHDL folds a basic
// identifier to lower case, and that is the spelling VPI sees.

#include <verilated.h>
#include <verilated_vpi.h>

#include VM_PREFIX_INCLUDE

#include <cstdio>
#include <cstring>

static int errors = 0;

#define CHECK(cond, what) \
    do { \
        if (!(cond)) { \
            printf("%%Error: %s:%d: %s\n", __FILE__, __LINE__, what); \
            ++errors; \
        } \
    } while (0)

static vpiHandle byName(const char* name) {
    return vpi_handle_by_name(const_cast<char*>(name), nullptr);
}

int main(int argc, char** argv) {
    VerilatedContext context;
    context.commandArgs(argc, argv);
    VM_PREFIX* const topp = new VM_PREFIX{&context};

    // A signal inside an instantiated entity, named as the hierarchy spells it
    vpiHandle countp = byName("TOP.t_vhdl_vpi.u_cnt.count");
    CHECK(countp, "cannot find count over VPI");
    vpiHandle flagp = byName("TOP.t_vhdl_vpi.u_cnt.flag");
    CHECK(flagp, "cannot find flag over VPI");
    // A generic reads back like a parameter does
    vpiHandle widthp = byName("TOP.t_vhdl_vpi.u_cnt.width");
    CHECK(widthp, "cannot find the generic over VPI");

    if (countp) {
        CHECK(vpi_get(vpiSize, countp) == 8, "count is not eight bits");
        CHECK(std::strcmp(vpi_get_str(vpiName, countp), "count") == 0, "count has another name");
    }
    if (flagp) CHECK(vpi_get(vpiSize, flagp) == 1, "flag is not one bit");
    if (widthp) {
        t_vpi_value val;
        val.format = vpiIntVal;
        vpi_get_value(widthp, &val);
        CHECK(val.value.integer == 8, "the generic does not read back as 8");
    }

    topp->rst = 1;
    topp->clk = 0;
    topp->eval();
    topp->rst = 0;
    for (int cycle = 0; cycle < 5; ++cycle) {
        topp->clk = 1;
        topp->eval();
        topp->clk = 0;
        topp->eval();
    }

    if (countp) {
        t_vpi_value val;
        val.format = vpiIntVal;
        vpi_get_value(countp, &val);
        CHECK(val.value.integer == 5, "count did not reach 5");
    }

    // Writing a signal takes effect at the next eval, as it does for Verilog
    if (countp) {
        t_vpi_value val;
        val.format = vpiIntVal;
        val.value.integer = 100;
        vpi_put_value(countp, &val, nullptr, vpiNoDelay);
        topp->clk = 1;
        topp->eval();
        topp->clk = 0;
        topp->eval();
        vpi_get_value(countp, &val);
        CHECK(val.value.integer == 101, "a value written over VPI did not take");
    }

    topp->final();
    delete topp;
    if (errors) {
        printf("%%Error: %d failures\n", errors);
        return 10;
    }
    printf("*-* All Finished *-*\n");
    return 0;
}
