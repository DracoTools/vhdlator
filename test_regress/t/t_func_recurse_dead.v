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
  // Never called, and calling only themselves or each other: dead, and not
  // emitted (as C++ they recurse without end, which compilers warn about)
  function automatic int spin(int arg);
    return spin(arg);
  endfunction
  function automatic int ping(int arg);
    return pong(arg);
  endfunction
  function automatic int pong(int arg);
    return ping(arg);
  endfunction
  // Recursive and called: kept
  function automatic int fact(int n);
    return n <= 1 ? 1 : n * fact(n - 1);
  endfunction
  function automatic int is_even(int n);
    return n == 0 ? 1 : is_odd(n - 1);
  endfunction
  function automatic int is_odd(int n);
    return n == 0 ? 0 : is_even(n - 1);
  endfunction

  initial begin
    `checkd(fact(5), 120);
    `checkd(is_even(10), 1);
    `checkd(is_odd(7), 1);
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
