// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A parameter that is an unpacked array of unpacked structures, given to an
// instance and passed on from there. Naming the specialization asked for the
// value of each structure as text, which had none (an internal error).

typedef struct {
  int a;
  int b;
} s_t;
typedef s_t arr_t[0:1];

module sub #(
    parameter arr_t P = '{'{a: 0, b: 0}, '{a: 0, b: 0}},
    parameter int A0 = 0,
    parameter int B1 = 0
) ();
  initial begin
    if (P[0].a != A0) $stop;
    if (P[1].b != B1) $stop;
  end
endmodule

module mid #(
    parameter arr_t P = '{'{a: 1, b: 2}, '{a: 3, b: 4}},
    parameter int A0 = 1,
    parameter int B1 = 4
) ();
  sub #(.P(P), .A0(A0), .B1(B1)) u ();
endmodule

module t;
  mid #(.P('{'{a: 5, b: 6}, '{a: 7, b: 8}}), .A0(5), .B1(8)) m ();
  mid d ();
  sub #(.P('{'{a: 9, b: 10}, '{a: 11, b: 12}}), .A0(9), .B1(12)) s ();
  initial begin
    #1;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
