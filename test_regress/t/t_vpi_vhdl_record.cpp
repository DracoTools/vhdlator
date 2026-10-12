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
#include <set>
#include <string>

#include "TestVpi.h"

struct Seen final {
    const char* name;
    int count = 0;
    int64_t lastTime = -1;
    int lastValue = 0;
};
static Seen s_a{"t_vpi_vhdl_record.s_ints.a"};
static Seen s_b{"t_vpi_vhdl_record.s_ints.b"};
static Seen s_v{"t_vpi_vhdl_record.s_ints.v"};
static Seen s_code{"t_vpi_vhdl_record.s_ints.n.code"};

static int onChange(p_cb_data cb_data) {
    Seen* const seenp = reinterpret_cast<Seen*>(cb_data->user_data);
    ++seenp->count;
    seenp->lastTime = Verilated::threadContextp()->time();
    seenp->lastValue = cb_data->value->value.integer;
    return 0;
}

static vpiHandle watch(Seen& seen, s_vpi_time& t, s_vpi_value& v) {
    vpiHandle const varh = vpi_handle_by_name(const_cast<PLI_BYTE8*>(seen.name), NULL);
    if (!varh) return nullptr;
    s_cb_data cb;
    cb.reason = cbValueChange;
    cb.cb_rtn = onChange;
    cb.obj = varh;
    cb.time = &t;
    cb.value = &v;
    cb.user_data = reinterpret_cast<PLI_BYTE8*>(&seen);
    return vpi_register_cb(&cb);
}

static int intOf(const char* name) {
    TestVpiHandle h = vpi_handle_by_name(const_cast<PLI_BYTE8*>(name), NULL);
    if (!h) return -999;
    s_vpi_value v;
    v.format = vpiIntVal;
    vpi_get_value(h, &v);
    return v.value.integer;
}

static int checkShape() {
    TestVpiHandle rech
        = vpi_handle_by_name(const_cast<PLI_BYTE8*>("t_vpi_vhdl_record.s_ints"), NULL);
    CHECK_RESULT_NZ(rech);
    // A structure, not a packed one: VHDL records are not packed
    CHECK_RESULT(vpi_get(vpiType, rech), vpiStructVar);
    CHECK_RESULT(vpi_get(vpiPacked, rech), 0);
    // Its fields, in order
    std::string names;
    TestVpiHandle iterh = vpi_iterate(vpiMember, rech);
    CHECK_RESULT_NZ(iterh);
    while (vpiHandle memh = vpi_scan(iterh)) {
        names += std::string{vpi_get_str(vpiName, memh)} + ";";
        vpi_release_handle(memh);
    }
    iterh.freed();
    CHECK_RESULT_CSTR(names.c_str(), "a;b;n;v;");
    TestVpiHandle vh
        = vpi_handle_by_name(const_cast<PLI_BYTE8*>("t_vpi_vhdl_record.s_ints.v"), NULL);
    CHECK_RESULT_NZ(vh);
    CHECK_RESULT(vpi_get(vpiSize, vh), 8);
    TestVpiHandle nh
        = vpi_handle_by_name(const_cast<PLI_BYTE8*>("t_vpi_vhdl_record.s_ints.n"), NULL);
    CHECK_RESULT_NZ(nh);
    CHECK_RESULT(vpi_get(vpiType, nh), vpiStructVar);
    // An integer field is an integer object, a vector field a vector
    TestVpiHandle ah
        = vpi_handle_by_name(const_cast<PLI_BYTE8*>("t_vpi_vhdl_record.s_ints.a"), NULL);
    CHECK_RESULT_NZ(ah);
    CHECK_RESULT(vpi_get(vpiType, ah), vpiIntegerVar);
    CHECK_RESULT(vpi_get(vpiType, vh), vpiReg);
    return 0;
}

static int checkValues() {
    // Each field's own value, read from the record's bits
    CHECK_RESULT(intOf("t_vpi_vhdl_record.s_ints.a"), 2);
    CHECK_RESULT(intOf("t_vpi_vhdl_record.s_ints.b"), 5);
    CHECK_RESULT(intOf("t_vpi_vhdl_record.s_ints.v"), 0xa5);
    CHECK_RESULT(intOf("t_vpi_vhdl_record.s_ints.n.code"), 0xc);
    CHECK_RESULT(intOf("t_vpi_vhdl_record.s_ints.n.flag"), 0);
    // Written through a field, only that field changes
    TestVpiHandle bh
        = vpi_handle_by_name(const_cast<PLI_BYTE8*>("t_vpi_vhdl_record.s_ints.b"), NULL);
    s_vpi_value v;
    v.format = vpiIntVal;
    v.value.integer = 77;
    vpi_put_value(bh, &v, NULL, vpiNoDelay);
    CHECK_RESULT(intOf("t_vpi_vhdl_record.s_ints.b"), 77);
    CHECK_RESULT(intOf("t_vpi_vhdl_record.s_ints.a"), 2);
    CHECK_RESULT(intOf("t_vpi_vhdl_record.s_ints.v"), 0xa5);
    return 0;
}

// VHDL's time resolution here is the femtosecond
static constexpr int64_t kNs = 1000000;

static int checkChanges() {
    // Each field changed once, when it was assigned, and not when another
    // field of the record was
    CHECK_RESULT(s_a.count, 1);
    CHECK_RESULT(s_a.lastTime, 10 * kNs);
    CHECK_RESULT(s_a.lastValue, 2);
    CHECK_RESULT(s_b.count, 1);
    CHECK_RESULT(s_b.lastTime, 20 * kNs);
    CHECK_RESULT(s_b.lastValue, 5);
    CHECK_RESULT(s_v.count, 1);
    CHECK_RESULT(s_v.lastTime, 20 * kNs);
    CHECK_RESULT(s_v.lastValue, 0xa5);
    CHECK_RESULT(s_code.count, 1);
    CHECK_RESULT(s_code.lastTime, 30 * kNs);
    CHECK_RESULT(s_code.lastValue, 0xc);
    return 0;
}

int main(int argc, char** argv) {
    const std::unique_ptr<VerilatedContext> contextp{new VerilatedContext};
    contextp->commandArgs(argc, argv);
    const std::unique_ptr<VM_PREFIX> topp{new VM_PREFIX{contextp.get(), ""}};
    if (const int line = checkShape()) {
        printf("%%Error: shape check failed at line %d\n", line);
        return 1;
    }
    s_vpi_time t;
    t.type = vpiSuppressTime;
    s_vpi_value v;
    v.format = vpiIntVal;
    TestVpiHandle cbs[4] = {watch(s_a, t, v), watch(s_b, t, v), watch(s_v, t, v),
                            watch(s_code, t, v)};
    while (!contextp->gotFinish()) {
        topp->eval();
        VerilatedVpi::callValueCbs();
        if (!topp->eventsPending()) break;
        contextp->time(topp->nextTimeSlot());
    }
    if (const int line = checkValues()) {
        printf("%%Error: value check failed at line %d\n", line);
        return 1;
    }
    topp->final();
    if (const int line = checkChanges()) {
        printf("%%Error: change check failed at line %d\n", line);
        return 1;
    }
    printf("*-* All Finished *-*\n");
    return 0;
}
