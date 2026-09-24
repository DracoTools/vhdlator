// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A dynamic array assigned to a fixed-size array must have its size when the
// assignment runs (IEEE 1800-2023 7.6)

module t;
  int d[];
  int f[4];

  initial begin
    d = new[3];
    f = d;
    $display("%0d", f[0]);
    $stop;
  end
endmodule
