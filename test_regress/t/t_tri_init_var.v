// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A variable with an initial value that a process later writes 'z' to: the
// initial value is what the variable holds until it is written, not a driver
// of its own -- it was or-ed into every value written after

module t;
  logic [7:0] s = 8'h12;
  wire [7:0] c;
  assign c = s;
  initial begin
    #1;
    if (s !== 8'h12 || c !== 8'h12) $stop;
    s <= 8'bzzzz0001;
    #1;
    // Two-state: 'z reads as zero
    if (s[3:0] !== 4'b0001 || c[3:0] !== 4'b0001) $stop;
    if (s[7:4] === 4'h1) $stop;
    s <= 8'h34;
    #1;
    if (s !== 8'h34 || c !== 8'h34) $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
