// DESCRIPTION: Verilator Example: the C half
//
// This file ONLY is placed under the Creative Commons Public Domain
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0
//
// Nothing here says VHDL. These are ordinary C functions, the same ones a
// SystemVerilog testbench would import over DPI, and one C model can serve
// both languages in a single binary.

#include "svdpi.h"

#include "Vtop__Dpi.h"

int c_popcount(int v) {
    int count = 0;
    unsigned u = static_cast<unsigned>(v);
    while (u) {
        count += static_cast<int>(u & 1);
        u >>= 1;
    }
    return count;
}

int c_add(int a, int b) { return a + b; }

void c_divmod(int num, int den, int* q, int* r) {
    *q = num / den;
    *r = num % den;
}
