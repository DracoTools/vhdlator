// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A parameter that is an unpacked array of unpacked structures. Emitting its
// definition asked whether the element's basic type is a C++ literal type,
// and the element has no basic type: a null dereference that crashed the
// optimized build.

module t;
  typedef struct {
    int a;
    string s;
  } s_t;
  localparam s_t P[2] = '{'{a: 1, s: "a"}, '{a: 2, s: "b"}};
  initial begin
    if (P[0].a != 1 || P[1].a != 2 || P[1].s != "b") $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
