// DESCRIPTION: Verilator: Verilog Test module
//
// An unpacked array whose range starts at the most negative int: the span
// of its range does not fit in an int, and a constant index into it is in
// range all the same.
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

module t;
  localparam int LO = 32'sh8000_0000;
  int a[LO : LO + 1];
  int b[32'sh8000_0000 : 32'sh8000_0002];

  initial begin
    a[LO] = 5;
    a[LO+1] = 6;
    b[32'sh8000_0000] = 1;
    b[32'sh8000_0001] = 7;
    b[32'sh8000_0002] = 9;
    if (a[LO] != 5 || a[LO+1] != 6) $stop;
    if (b[32'sh8000_0000] != 1 || b[32'sh8000_0001] != 7 || b[32'sh8000_0002] != 9) $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
