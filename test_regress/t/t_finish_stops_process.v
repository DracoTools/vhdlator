// DESCRIPTION: Verilator: Verilog Test module
//
// $finish ends the run where it is called: nothing after it in the
// calling function, task, method or process runs
//
// This file ONLY is placed under the Creative Commons Public Domain
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

module t;
  int x;

  function automatic int f(int n);
    $display("f before %0d", n);
    if (n == 2) $finish;
    $display("f after %0d", n);
    return n + 1;
  endfunction

  task automatic tk(int n);
    $display("tk before %0d", n);
    x = f(n);
    $display("tk after %0d", n);
  endtask

  class C;
    task m(int n);
      tk(n);
      $display("m after %0d", n);
    endtask
  endclass

  initial begin
    C c;
    c = new;
    $display("initial before");
    c.m(1);
    for (int i = 2; i < 4; ++i) begin
      c.m(i);
      $display("loop after %0d", i);
    end
    $display("initial after");
    $finish;
  end

  final $display("final");
endmodule
