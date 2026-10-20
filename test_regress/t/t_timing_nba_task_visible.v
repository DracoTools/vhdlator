// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A nonblocking assignment made in a task that is not inlined, and one made in
// its caller after it, in the same time step: both take effect in the same NBA
// region, so a process the second one resumes sees the first one's update too
// (IEEE 1800-2023 4.4.2.2, 4.6). The waiting process is set up first, so that it
// would be resumed ahead of the task's assignment.

module t;
  logic [7:0] a = 0;
  logic [7:0] ac;
  logic b = 0;

  // What the waiter reads, through a continuous assignment as well
  assign ac = a;

  task automatic wait_b();
    /*verilator no_inline_task*/
    wait (b == 1'b1);
  endtask

  task automatic set_a(input logic [7:0] v);
    /*verilator no_inline_task*/
    a <= v;
    #0;
  endtask

  initial begin
    wait_b();
    if (a !== 8'd5) $stop;
    if (ac !== 8'd5) $stop;
    // Again, with the task's update the only one in the time step
    #2;
    if (a !== 8'd6 || ac !== 8'd6) $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end

  initial begin
    #1;
    set_a(8'd5);
    b <= 1'b1;
    #1;
    set_a(8'd6);
  end
endmodule
