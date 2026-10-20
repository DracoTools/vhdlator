// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// Nonblocking assignments to one variable made by a process and by a task that
// is not inlined, in the same time step and in either order: the updates are
// made in the order the assignments were executed (IEEE 1800-2023 10.4.2), so
// the one executed last stands. The task's is made by a fork that waits for the
// NBA event, the process's had been made by V3Delayed, after it.

`define stop $stop
`define checkd(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got=%0d exp=%0d\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0);

module t;
  typedef struct packed {
    int rdy;
    int ack;
  } rec_t;
  rec_t a[1:2];
  int idx = 1;
  int q;

  // Assigns, then suspends
  task automatic req(input int v);
    /*verilator no_inline_task*/
    a[1].rdy <= v;
    #0;
  endtask
  // Suspends, then assigns and returns
  task automatic late(input int v);
    /*verilator no_inline_task*/
    #0;
    a[1].rdy <= v;
    q <= v;
  endtask

  initial begin
    a[1].rdy = 7;
    a[2].rdy = 0;
    #1;
    // The process's first, at an index only running settles, then the task's
    for (int k = idx; k <= idx; ++k) a[k].rdy <= a[k].rdy;
    req(8);
    #1;
    `checkd(a[1].rdy, 8);
    // The task's first, then the process's
    late(30);
    a[idx].rdy <= 31;
    q <= 32;
    #1;
    `checkd(a[1].rdy, 31);
    `checkd(q, 32);
    // The process's, then the task's after it suspends
    a[1].rdy <= 40;
    late(41);
    #1;
    `checkd(a[1].rdy, 41);
    `checkd(q, 41);
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
