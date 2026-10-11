// DESCRIPTION: Verilator: Verilog Test module
//
// $finish in an always block that suspends: the rest of the body does not
// run, and the block does not start again
//
// This file ONLY is placed under the Creative Commons Public Domain
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

module t;
  int n;

  task automatic check(int k);
    if (k == 3) $finish;
  endtask

  always begin
    $display("top %0d", n);
    #1;
    n++;
    check(n);
    $display("bottom %0d", n);
  end

  final $display("final n=%0d", n);
endmodule
