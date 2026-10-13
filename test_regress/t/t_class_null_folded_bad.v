// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

class C;
  int a;
endclass

module t;
  // Known to be null by constant propagation (V3Gate here, V3Life for a
  // local assigned null): the member read still compiles, and fails at run
  // time on its own line
  C m = null;
  int v;
  initial begin
    $display("before");
    v = m.a;
    $display("not reached %0d", v);
    $finish;
  end
endmodule
