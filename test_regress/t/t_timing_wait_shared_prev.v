// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A process changes a signal and then waits on another edge of it. Checking
// whether that wait is already satisfied reads the signal's previous value,
// which the triggers of an always block on the same signal share: the
// change has to reach that block too, not only the waiting process.

module t;
  logic s = 0;
  time te = 0;
  logic ps = 0;
  int n = 0;

  always @(posedge s or negedge s) begin
    te = $time;
    ps = s;
    n++;
  end

  initial begin
    #10ns s = 1;
    @(negedge s);
  end

  initial begin
    #5ns s = 0;
    #10ns;
    if (te != 10ns) $stop;
    if (ps !== 1'b1) $stop;
    if (n != 1) $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
