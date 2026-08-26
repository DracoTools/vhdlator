// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// Each execution of "d <= #3 y" samples y where it stands and delivers that
// value three units later, so three executions three units apart deliver
// three different values. Sharing one slot for the value in flight loses all
// but the last.

module t;

  int y = 0;
  int d = 0;
  int seen[$];

  initial begin
    #1 y = 1;
    #1 y = 2;
    #1 y = 3;
  end

  always @(y) d <= #3 y;

  initial begin
    for (int i = 1; i <= 8; ++i) begin
      #1 seen.push_back(d);
    end
    // d is read in the active region, so each value shows one step after it
    // is delivered: 0 0 0 0 1 2 3 3
    if (seen[3] !== 0) $stop;
    if (seen[4] !== 1) $stop;
    if (seen[5] !== 2) $stop;
    if (seen[6] !== 3) $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end

endmodule
