// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// An 'inout' or 'ref' argument reads the actual as well as writing it. These
// variables are read nowhere else, so if the connection is recorded as
// write-only their initial values look dead and are dropped.

module t;

  int a = 4;
  int b = 7;
  int v;

  task automatic bump(inout int x, output int o);
    x = x + 1;
    o = x;
  endtask

  task automatic bumpref(ref int x);
    x = x + 1;
  endtask

  initial begin
    bump(a, v);
    if (v !== 5) $stop;
    if (a !== 5) $stop;
    bumpref(b);
    if (b !== 8) $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end

endmodule
