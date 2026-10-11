// DESCRIPTION: Verilator: Verilog Test module
//
// A variable's declared initial value is not a change a value change
// callback reports; an assignment in an initial block at time zero is
//
// This file ONLY is placed under the Creative Commons Public Domain
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

module t;
  int declared /*verilator public_flat_rw*/ = 5;
  int assigned /*verilator public_flat_rw*/;

  initial begin
    assigned = 7;
    #1;
    declared = 6;
    #1;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
