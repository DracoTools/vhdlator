// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A +: slice of an unpacked array needs a constant base.  The point of the
// test is that saying so is all that happens: leaving the select in the tree
// used to turn the message below into an internal error.

module t;
  typedef int t2 [0:1];
  t2 arr [0:7];
  t2 v [0:7];
  initial begin
    for (int i = 0; i < 2; i++) begin
      v = arr[i*4 +: 4];
    end
  end
endmodule
