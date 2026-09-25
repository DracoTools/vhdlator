// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A task kept out of line whose body is a fork: the fork becomes a private
// method of the class, and the task's small body was then inlined into a
// function that is not a method, which cannot call it
module t;
  logic s = 0;
  int n = 0;
  task automatic bump();
    // verilator no_inline_task
    fork
      #1 s = 1;
    join_none
  endtask
  task automatic count();
    // verilator no_inline_task
    fork
      #1 n = n + 1;
    join_none
  endtask
  initial begin
    bump();
    count();
    #2;
    if (s !== 1 || n !== 1) $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
