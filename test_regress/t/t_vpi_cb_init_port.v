// DESCRIPTION: Verilator: Verilog Test module
//
// A top-level output's declared initial value is where the port starts, as
// seen through VPI: not a change a value change callback reports
//
// This file ONLY is placed under the Creative Commons Public Domain
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

module t (
    output int declared  /*verilator public_flat_rw*/ = 5
);
  initial begin
    #1;
    declared = 6;
    #1;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
