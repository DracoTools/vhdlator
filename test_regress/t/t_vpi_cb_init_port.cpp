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

struct Seen final {
    const char* name;
    int count = 0;
    int64_t lastTime = -1;
    int lastValue = 0;
};
static Seen s_declared{"declared"};

static int onChange(p_cb_data cb_data) {
    Seen* const seenp = reinterpret_cast<Seen*>(cb_data->user_data);
    ++seenp->count;
    seenp->lastTime = Verilated::threadContextp()->time();
    seenp->lastValue = cb_data->value->value.integer;
    return 0;
}

static void watch(Seen& seen, TestVpiHandle& cbh, s_vpi_time& t, s_vpi_value& v) {
    TestVpiHandle varh = vpi_handle_by_name(const_cast<PLI_BYTE8*>(seen.name),
                                            vpi_handle_by_name(const_cast<PLI_BYTE8*>("t"),
                                                               NULL));
    s_cb_data cb;
    cb.reason = cbValueChange;
    cb.cb_rtn = onChange;
    cb.obj = varh;
    cb.time = &t;
    cb.value = &v;
    cb.user_data = reinterpret_cast<PLI_BYTE8*>(&seen);
    cbh = vpi_register_cb(&cb);
}

static int check() {
    // The declared value is where the port starts, outside the module as
    // well as inside it, so the one change is at time 1
    CHECK_RESULT(s_declared.count, 1);
    CHECK_RESULT(s_declared.lastTime, 1);
    CHECK_RESULT(s_declared.lastValue, 6);
    return 0;
}

int main(int argc, char** argv) {
    const std::unique_ptr<VerilatedContext> contextp{new VerilatedContext};
    contextp->commandArgs(argc, argv);
    const std::unique_ptr<VM_PREFIX> topp{new VM_PREFIX{contextp.get(), ""}};
    // Registered before the first evaluation, as a testbench does at the
    // start of simulation
    s_vpi_time t;
    t.type = vpiSuppressTime;
    s_vpi_value v;
    v.format = vpiIntVal;
    TestVpiHandle cbDeclared;
    watch(s_declared, cbDeclared, t, v);
    while (!contextp->gotFinish()) {
        topp->eval();
        VerilatedVpi::callValueCbs();
        if (!topp->eventsPending()) break;
        contextp->time(topp->nextTimeSlot());
    }
    topp->final();
    if (const int line = check()) {
        printf("%%Error: check failed at line %d\n", line);
        return 1;
    }
    return 0;
}
