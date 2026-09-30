// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0
//
// VPI lookup of VHDL names in a spelling other than the stored lower case,
// and of Verilog names in the same design, which must match exactly.

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

static bool sameName(vpiHandle h, const char* name) {
    return h && std::strcmp(vpi_get_str(vpiName, h), name) == 0;
}

int main(int argc, char** argv) {
    VerilatedContext context;
    context.commandArgs(argc, argv);
    // Named as cocotb names it, so a scope's full name is the path a lookup under it takes
    VM_PREFIX* const topp = new VM_PREFIX{&context, ""};
    topp->clk = 0;
    topp->eval();

    // The stored spelling
    vpiHandle exactp = byName("t_vhdl_vpicase.u_leaf.shortname");
    CHECK(exactp, "shortname not found as stored");
    // The source's spelling, and others, in every part of the name
    vpiHandle srcp = byName("t_vhdl_vpicase.U_LEAF.ShortName");
    CHECK(sameName(srcp, "shortname"), "U_LEAF.ShortName not found");
    vpiHandle upperp = byName("T_VHDL_VPICASE.u_leaf.SHORTNAME");
    CHECK(sameName(upperp, "shortname"), "T_VHDL_VPICASE.u_leaf.SHORTNAME not found");
    if (upperp) CHECK(intValue(upperp) == 5, "SHORTNAME does not read 5");
    // A scope, found as one
    vpiHandle leafp = byName("t_vhdl_vpicase.U_Leaf");
    CHECK(leafp && vpi_get(vpiType, leafp) == vpiModule, "U_Leaf is not found as a module");
    // A name relative to a scope handle, as cocotb asks for a child
    if (leafp) CHECK(sameName(byName("ShortName", leafp), "shortname"), "child ShortName");
    // A generic, and a port
    vpiHandle depthp = byName("t_vhdl_vpicase.u_leaf.Depth");
    CHECK(sameName(depthp, "depth"), "generic Depth not found");
    if (depthp) CHECK(intValue(depthp) == 3, "generic Depth does not read 3");
    CHECK(byName("t_vhdl_vpicase.U_LEAF.Clk"), "port Clk not found");
    // A name that is not there is still not found
    CHECK(!byName("t_vhdl_vpicase.u_leaf.LongName"), "LongName found");

    // The Verilog module's own names are exact: Foo and foo are two signals
    vpiHandle bigp = byName("t_vhdl_vpicase.U_V.Foo");
    vpiHandle smallp = byName("t_vhdl_vpicase.u_v.foo");
    CHECK(sameName(bigp, "Foo"), "Verilog Foo not found");
    CHECK(sameName(smallp, "foo"), "Verilog foo not found");
    if (bigp) CHECK(intValue(bigp) == 1, "Verilog Foo does not read 1");
    if (smallp) CHECK(intValue(smallp) == 0, "Verilog foo does not read 0");
    CHECK(!byName("t_vhdl_vpicase.u_v.FOO"), "Verilog FOO found, but no signal has that name");

    topp->final();
    delete topp;
    if (errors) {
        printf("%%Error: %d failures\n", errors);
        return 10;
    }
    printf("*-* All Finished *-*\n");
    return 0;
}
