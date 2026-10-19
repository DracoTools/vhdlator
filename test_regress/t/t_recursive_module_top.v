// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// The top module instantiates itself, ending where a parameter says. Named with
// --top-module, its own instance of itself was taken for a parent's instance of
// it and deleted, so the hierarchy stopped at the top with no message, and the
// leaf, which ends the run, did not exist.

module t #(
    parameter int DEPTH = 3
) ();
  initial $display("depth %0d", DEPTH);
  if (DEPTH > 0) begin : g
    t #(.DEPTH(DEPTH - 1)) sub ();
  end else begin : leaf
    initial begin
      #1;
      $write("*-* All Finished *-*\n");
      $finish;
    end
  end
endmodule
