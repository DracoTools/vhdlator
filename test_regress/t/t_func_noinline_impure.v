// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A task left out of line is still a statement of whoever calls it, so it may
// read and write whatever the caller may, however deep the call goes.

module t;

  int sig;

  task foo;
    // verilator no_inline_task
    sig = sig + 1;
  endtask

  task bar;
    sig = sig + 10;
  endtask

  task baz;
    // verilator no_inline_task
    bar();
  endtask

  initial begin
    sig = 0;
    foo();
    baz();
    if (sig != 11) $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end

endmodule
