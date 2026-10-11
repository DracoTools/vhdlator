// DESCRIPTION: Verilator: Verilog Test module
//
// A row of a two-dimensional unpacked localparam handed to a parameter
//
// This file ONLY is placed under the Creative Commons Public Domain
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

`define stop $stop
`define checkh(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got='h%x exp='h%x\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0);

module sub #(
    parameter logic [7:0] INIT[0:3] = '{default: 8'h0},
    parameter logic [7:0] EXP0 = 8'h0,
    parameter logic [7:0] EXP3 = 8'h0
);
  initial begin
    `checkh(INIT[0], EXP0);
    `checkh(INIT[3], EXP3);
  end
endmodule

module t;
  localparam logic [7:0] TBL[0:1][0:3] = '{'{8'h1, 8'h2, 8'h3, 8'h4}, '{8'h5, 8'h6, 8'h7, 8'h8}};
  localparam logic [7:0] ZERO[0:2][0:3] = '{default: '{default: 8'h0}};

  for (genvar i = 0; i < 2; i++) begin : g
    sub #(.INIT(TBL[i]), .EXP0(8'(i * 4 + 1)), .EXP3(8'(i * 4 + 4))) u ();
  end
  sub #(.INIT(ZERO[2])) uz ();

  initial begin
    #1;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
