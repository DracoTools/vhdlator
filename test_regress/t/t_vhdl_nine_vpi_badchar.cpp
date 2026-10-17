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

int main(int argc, char** argv) {
    VerilatedContext context;
    context.commandArgs(argc, argv);
    VM_PREFIX* const topp = new VM_PREFIX{&context};
    topp->eval();

    vpiHandle const inp
        = vpi_handle_by_name(const_cast<char*>("TOP.t_vhdl_nine_vpi.inp"), nullptr);
    if (!inp) {
        printf("%%Error: cannot find inp\n");
        return 10;
    }
    // A character that is none of the nine is written as 'X', with a warning
    t_vpi_value val;
    val.format = vpiBinStrVal;
    val.value.str = const_cast<char*>("01Q1");
    vpi_put_value(inp, &val, nullptr, vpiNoDelay);
    s_vpi_error_info info;
    if (vpi_chk_error(&info)) printf("%s\n", info.message);
    topp->eval();
    val.format = vpiBinStrVal;
    vpi_get_value(inp, &val);
    printf("inp = %s\n", val.value.str);

    printf("*-* All Finished *-*\n");
    topp->final();
    delete topp;
    return 0;
}
