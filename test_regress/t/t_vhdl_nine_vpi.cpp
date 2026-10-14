// DESCRIPTION: Verilator: VHDL VPI test
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

#include <verilated.h>
#include <verilated_vpi.h>

#include VM_PREFIX_INCLUDE

#include <cstdio>
#include <cstring>
#include <string>

static int errors = 0;

#define CHECK(cond, what) \
    do { \
        if (!(cond)) { \
            printf("%%Error: %s:%d: %s\n", __FILE__, __LINE__, what); \
            ++errors; \
        } \
    } while (0)

static vpiHandle byName(const char* name) {
    vpiHandle const h = vpi_handle_by_name(const_cast<char*>(name), nullptr);
    if (!h) printf("%%Error: cannot find %s\n", name);
    if (!h) ++errors;
    return h;
}
static std::string str(vpiHandle h, PLI_INT32 format) {
    t_vpi_value val;
    val.format = format;
    vpi_get_value(h, &val);
    return val.value.str ? val.value.str : "";
}
static int integer(vpiHandle h) {
    t_vpi_value val;
    val.format = vpiIntVal;
    vpi_get_value(h, &val);
    return val.value.integer;
}
static void putStr(vpiHandle h, PLI_INT32 format, const char* text) {
    t_vpi_value val;
    val.format = format;
    val.value.str = const_cast<char*>(text);
    vpi_put_value(h, &val, nullptr, vpiNoDelay);
}

int main(int argc, char** argv) {
    VerilatedContext context;
    context.commandArgs(argc, argv);
    VM_PREFIX* const topp = new VM_PREFIX{&context};
    topp->eval();

    vpiHandle const neverp = byName("TOP.t_vhdl_nine_vpi.never");
    vpiHandle const mixp = byName("TOP.t_vhdl_nine_vpi.mix");
    vpiHandle const sp = byName("TOP.t_vhdl_nine_vpi.s");
    vpiHandle const inp = byName("TOP.t_vhdl_nine_vpi.inp");
    vpiHandle const resp = byName("TOP.t_vhdl_nine_vpi.res");
    if (errors) return 10;

    // An element is one bit to VPI
    CHECK(vpi_get(vpiSize, neverp) == 4, "never is not four elements");
    CHECK(vpi_get(vpiSize, mixp) == 9, "mix is not nine elements");
    CHECK(vpi_get(vpiSize, sp) == 1, "s is not one element");

    // Read: the nine characters, and 0/1/x/z where VPI has only those
    CHECK(str(neverp, vpiBinStrVal) == "UUUU", "never does not read UUUU");
    CHECK(str(mixp, vpiBinStrVal) == "UX01ZWLH-", "mix does not read UX01ZWLH-");
    CHECK(str(sp, vpiBinStrVal) == "Z", "s does not read Z");
    {
        t_vpi_value val;
        val.format = vpiScalarVal;
        vpi_get_value(sp, &val);
        CHECK(val.value.scalar == vpiZ, "s is not vpiZ");
        val.format = vpiVectorVal;
        vpi_get_value(mixp, &val);
        // From the right: '-' x, 'H' 1, 'L' 0, 'W' x, 'Z' z, '1' 1, '0' 0, 'X' x, 'U' x
        CHECK(val.value.vector[0].aval == 427, "mix has the wrong aval");
        CHECK(val.value.vector[0].bval == 409, "mix has the wrong bval");
    }
    CHECK(str(mixp, vpiHexStrVal) == "xxx", "mix does not read xxx in hex");

    // Write: a binary string of the nine characters
    putStr(inp, vpiBinStrVal, "01HL");
    topp->eval();
    CHECK(str(inp, vpiBinStrVal) == "01HL", "inp does not read back 01HL");
    CHECK(str(resp, vpiBinStrVal) == "1001", "not 01HL is not 1001");
    CHECK(integer(resp) == 9, "1001 does not read as 9");

    // An integer: '0' and '1'
    {
        t_vpi_value val;
        val.format = vpiIntVal;
        val.value.integer = 5;
        vpi_put_value(inp, &val, nullptr, vpiNoDelay);
    }
    topp->eval();
    CHECK(str(inp, vpiBinStrVal) == "0101", "5 does not write 0101");
    CHECK(str(resp, vpiBinStrVal) == "1010", "not 0101 is not 1010");
    CHECK(str(resp, vpiHexStrVal) == "a", "1010 does not read a in hex");
    CHECK(str(resp, vpiDecStrVal) == "10", "1010 does not read 10 in decimal");

    // A vector: aval and bval give 0, 1, Z and X
    {
        s_vpi_vecval vec[1];
        vec[0].aval = 0x3;
        vec[0].bval = 0x5;
        t_vpi_value val;
        val.format = vpiVectorVal;
        val.value.vector = vec;
        vpi_put_value(inp, &val, nullptr, vpiNoDelay);
    }
    topp->eval();
    CHECK(str(inp, vpiBinStrVal) == "0Z1X", "the vector does not write 0Z1X");
    CHECK(str(resp, vpiBinStrVal) == "1X0X", "not 0Z1X is not 1X0X");
    CHECK(str(resp, vpiDecStrVal) == "x", "1X0X does not read x in decimal");

    // A scalar: 'H'
    {
        t_vpi_value val;
        val.format = vpiScalarVal;
        val.value.scalar = vpiH;
        vpi_put_value(sp, &val, nullptr, vpiNoDelay);
    }
    topp->eval();
    CHECK(str(sp, vpiBinStrVal) == "H", "s does not read back H");

    // Hexadecimal, with a z digit
    putStr(inp, vpiHexStrVal, "z");
    topp->eval();
    CHECK(str(inp, vpiBinStrVal) == "ZZZZ", "z does not write ZZZZ");
    CHECK(str(resp, vpiBinStrVal) == "XXXX", "not ZZZZ is not XXXX");

    topp->final();
    delete topp;
    if (errors) {
        printf("%%Error: %d failures\n", errors);
        return 10;
    }
    printf("*-* All Finished *-*\n");
    return 0;
}
