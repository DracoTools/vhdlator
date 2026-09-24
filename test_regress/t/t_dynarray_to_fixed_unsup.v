// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A dynamic array of dynamic arrays is not copied into a fixed-size array of
// fixed-size arrays

module t;
  int dd[][];
  int ff[2][2];

  initial begin
    ff = dd;
  end
endmodule
