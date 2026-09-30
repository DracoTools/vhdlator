// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0
//
// vpi_handle_by_name relative to a scope handle, in a model constructed
// with its default name, TOP.

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

static vpiHandle byName(const char* name, vpiHandle scope = nullptr) {
    return vpi_handle_by_name(const_cast<char*>(name), scope);
}

static int intValue(vpiHandle h) {
    t_vpi_value val;
    val.format = vpiIntVal;
    vpi_get_value(h, &val);
    return val.value.integer;
}

int main(int argc, char** argv) {
    VerilatedContext context;
    context.commandArgs(argc, argv);
    VM_PREFIX* const topp = new VM_PREFIX{&context};  // Named TOP
    topp->clk = 0;
    topp->eval();

    vpiHandle subp = byName("TOP.t.u_sub");
    CHECK(subp, "u_sub not found");
    vpiHandle tp = byName("TOP.t");
    CHECK(tp, "t not found");
    if (subp) {
        vpiHandle xp = byName("x", subp);
        CHECK(xp, "x not found under u_sub");
        if (xp) CHECK(intValue(xp) == 5, "x does not read 5");
        CHECK(!byName("nothere", subp), "nothere found under u_sub");
    }
    if (tp) {
        vpiHandle yp = byName("y", tp);
        CHECK(yp, "y not found under t");
        if (yp) CHECK(intValue(yp) == 9, "y does not read 9");
        vpiHandle nestedp = byName("u_sub.x", tp);
        CHECK(nestedp, "u_sub.x not found under t");
        if (nestedp) CHECK(intValue(nestedp) == 5, "u_sub.x does not read 5");
        CHECK(byName("u_sub", tp), "u_sub not found under t");
        CHECK(byName("clk", tp), "port clk not found under t");
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
