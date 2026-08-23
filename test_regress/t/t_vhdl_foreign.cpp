// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0
//
// The C half of t_vhdl_foreign. Nothing here says VHDL: these are ordinary C
// functions, the same ones a SystemVerilog testbench would import over DPI.

#include "svdpi.h"

#include "Vt_vhdl_foreign__Dpi.h"

int c_popcount(int v) {
    int count = 0;
    unsigned int u = static_cast<unsigned int>(v);
    while (u) {
        count += static_cast<int>(u & 1);
        u >>= 1;
    }
    return count;
}

int c_triple(int v) { return v * 3; }

int c_add(int a, int b) { return a + b; }

void c_divmod(int num, int den, int* q, int* r) {
    *q = num / den;
    *r = num % den;
}
