// DESCRIPTION: Verilator: Verilog Test module
//
// A constant function may not call a task (IEEE 1800-2023 13.4.4). A task
// lowered from VHDL is run when a constant function calls it, since a VHDL
// function may call a procedure; one written in Verilog is still refused.
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

module t;
  function automatic int f(int a);
    int r;
    g(a, r);
    return r;
  endfunction
  task automatic g(input int a, output int r);
    r = a + 1;
  endtask
  localparam P = f(2);
  initial begin
    if (P != 3) $stop;
    $finish;
  end
endmodule
