// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of either the GNU Lesser General Public License Version 3
// or the Perl Artistic License Version 2.0.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
//
//*************************************************************************

#include "verilated.h"
#include "verilated_vpi.h"

#include VM_PREFIX_INCLUDE

#include <cstdio>
#include <cstring>
#include <iostream>

#include "TestVpi.h"

static int check() {
    TestVpiHandle it = vpi_iterate(vpiModule, NULL);
    CHECK_RESULT_NZ(it);
    TestVpiHandle topmod = vpi_scan(it);
    CHECK_RESULT_NZ(topmod);
    CHECK_RESULT_CSTR(vpi_get_str(vpiName, topmod), "t");
    TestVpiHandle byNamep = vpi_handle_by_name(const_cast<PLI_BYTE8*>("t"), NULL);
    CHECK_RESULT_NZ(byNamep);
    return 0;
}

int main(int argc, char** argv) {
    const std::unique_ptr<VerilatedContext> contextp{new VerilatedContext};
    contextp->commandArgs(argc, argv);
    const std::unique_ptr<VM_PREFIX> topp{new VM_PREFIX{contextp.get(), ""}};
    if (const int line = check()) {
        printf("%%Error: check failed at line %d\n", line);
        return 1;
    }
    while (!contextp->gotFinish()) {
        topp->eval();
        VerilatedVpi::callValueCbs();
        if (!topp->eventsPending()) break;
        contextp->time(topp->nextTimeSlot());
    }
    topp->final();
    return 0;
}
