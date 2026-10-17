// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A constant of one unpacked structure initializing a parameter of another
// with the same members: a module's own copy of a structure whose width is a
// parameter, given the package's (the VHDL frontend's records). Verilator
// accepts the assignment; the C++ initializer named the value's structure
// where the parameter's was wanted, and did not compile.

typedef struct {
  logic [0:0] addr;
  int width;
} g_t;

module sub #(
    parameter int W = 1,
    parameter g_t Q[0:1] = '{default: '{addr: 0, width: 8}},
    parameter g_t S = '{addr: 1, width: 7}
) ();
  typedef struct {
    logic [W-1:0] addr;
    int width;
  } l_t;
  localparam l_t P[0:1] = Q;
  localparam l_t R = S;
  initial begin
    if (P[0].width != 9 || P[1].width != 10 || P[1].addr != 1'b1) $stop;
    if (R.width != 7 || R.addr != 1'b1) $stop;
  end
endmodule

module t;
  localparam g_t C[0:1] = '{'{addr: 0, width: 9}, '{addr: 1, width: 10}};
  sub #(.W(1), .Q(C)) u ();
  initial begin
    #1;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
