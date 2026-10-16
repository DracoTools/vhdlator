// DESCRIPTION: Verilator: Verilog Test module
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of either the GNU Lesser General Public License Version 3
// or the Perl Artistic License Version 2.0.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

// A process's NBA must not be lost when a fork...join_none branch it started
// resumes before the NBA commits: the branch's timing does not resume the parent.

`define stop $stop
`define checkd(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got=%0d exp=%0d\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0);

module t;
  logic clk = 0;
  int a = 0;
  int b = 0;
  int edges = 0;
  time d = 1;

  always #5 clk <= ~clk;

  task automatic wave(int v, time dd);
    fork
      #dd b <= v;
    join_none
  endtask

  always @(posedge clk) begin
    a <= a + 1;
    wave(a, d);
  end

  always @(negedge clk) begin
    edges++;
    `checkd(a, edges);
    `checkd(b, edges - 1);
    if (edges == 5) begin
      $write("*-* All Finished *-*\n");
      $finish;
    end
  end
endmodule
