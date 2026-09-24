// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A cast as the index of a queue or dynamic array element: the element
// became a method call that is not sized again, and the cast in its index
// was left for the emitter, which stopped on an internal error.

// verilog_format: off
`define stop $stop
`define checkh(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got='h%x exp='h%x\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0)
// verilog_format: on

module t;
  int q[$];
  int d[];
  logic [2:0] e = 3'd2;
  int n = 1;

  initial begin
    q = {10, 20, 30, 40};
    d = new[4];
    d[2] = 7;
    `checkh(q[int'(e)], 30);
    `checkh(d[int'(e)], 7);
    `checkh(q[3'(n)], 20);
    `checkh(q[int'(q.size()) - 2], 30);
    q[int'(e)] = 33;
    d[3'(n)] = 11;
    `checkh(q[2], 33);
    `checkh(d[1], 11);
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
