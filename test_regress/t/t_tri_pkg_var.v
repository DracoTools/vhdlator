// DESCRIPTION: Verilator: Verilog Test module
//
// A package's variable compared with a value holding z from a module: the
// enable the comparison reads is declared beside the variable, in the
// package, and is named through it.
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

package p;
  logic [3:0] s;
endpackage

module t;
  initial begin
    p::s = 4'b1100;
    #1;
    if (p::s !== 4'b1100) $stop;
    if (p::s === 4'b01zz) $stop;
    p::s = 4'b0001;
    #1;
    if (p::s !== 4'b0001) $stop;
    if (p::s === 4'bzzz1) $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
