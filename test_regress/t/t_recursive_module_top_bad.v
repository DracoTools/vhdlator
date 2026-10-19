// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A recursion through another module back to the top. Recursion through more
// than one module is unsupported, and with --top-module naming the top it was
// not reported: the instance of the top was deleted instead.

module t #(
    parameter int DEPTH = 2
) ();
  sub #(.DEPTH(DEPTH)) s ();
endmodule

module sub #(
    parameter int DEPTH = 0
) ();
  if (DEPTH > 0) begin : g
    t #(.DEPTH(DEPTH - 1)) u ();
  end
endmodule
