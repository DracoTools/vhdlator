// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A constructor whose parameter defaults to a value that does not fold to
// a constant, an unpacked assignment pattern, left out of the call

class Box;
  int all[1:2];
  int n;
  function new(int p[1:2] = '{1, 2}, int k = 7);
    all = p;
    n = k;
  endfunction
endclass

module t;
  Box b;
  Box c;
  initial begin
    b = new();
    c = new('{3, 4});
    if (b.all[1] != 1 || b.all[2] != 2 || b.n != 7) $stop;
    if (c.all[1] != 3 || c.all[2] != 4 || c.n != 7) $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
