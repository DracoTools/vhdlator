// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// verilog_format: off
`define stop $stop
`define checkd(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got=%0d exp=%0d\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0);
// verilog_format: on

module t;
  // A function named like process::self, in a module rather than a class
  function automatic int self(int arg);
    return arg + 1;
  endfunction

  initial begin
    #1;
    `checkd(self(41), 42);
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
