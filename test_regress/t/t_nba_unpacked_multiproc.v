// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// Two processes assign one element of an unpacked array by nonblocking
// assignment in one time slot: the one executed last is the one committed
// (IEEE 1800-2023 10.4.2), whichever comes first in the source

module t;
  logic [31:0] e[2:0];
  logic [31:0] w[2:0];
  logic [31:0] c;
  logic ev = 0;
  always @(ev) begin
    e[1][7:0] <= 8'h22;
    w[2] <= 32'h22;
    c[7:0] <= 8'h22;
  end
  initial begin
    e[1][7:0] <= 8'h11;
    w[2] <= 32'h11;
    c[7:0] <= 8'h11;
    ev = 1;
  end
  initial begin
    #1;
    if (e[1][7:0] !== 8'h22) $stop;
    if (w[2] !== 32'h22) $stop;
    if (c[7:0] !== 8'h22) $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
