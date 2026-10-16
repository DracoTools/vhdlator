// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A constructor whose parameter defaults to a value naming another
// parameter: worked out at the call, it would name the wrong thing

class Box;
  int all[1:2];
  function new(int a, int p[1:2] = '{a, a});
    all = p;
  endfunction
endclass

module t;
  Box b;
  initial begin
    b = new(5);
    $finish;
  end
endmodule
