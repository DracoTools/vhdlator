// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// "disable fork" ends the processes the calling process forked. An update a
// nonblocking assignment has already scheduled is not one of them: it is an
// update the scheduler owes, and it still happens.

module t;

  logic [7:0] x = 0;
  logic [7:0] y = 0;
  int steps = 0;

  initial begin
    x <= #5 8'hAA;
    fork
      begin
        #20;
        y <= 8'hBB;   // Must not happen: this branch is disabled
      end
    join_none
    fork
      #1 steps = steps + 1;
    join_any
    disable fork;
    #10;
    if (x !== 8'hAA) $stop;   // Scheduled before the disable, still owed
    if (steps !== 1) $stop;
    #20;
    if (y !== 8'h00) $stop;   // A forked process is ended, and stays ended
    $write("*-* All Finished *-*\n");
    $finish;
  end

endmodule
