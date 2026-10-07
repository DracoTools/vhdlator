// DESCRIPTION: Verilator: Verilog Test module
//
// A constant function filling a wide parameter a part at a time. Each
// assignment to part of the result must not cost a copy of the whole value
// a bit at a time, which made this quadratic in the width.
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

module t;
  localparam int WORDS = 16384;
  localparam int W = 16 * WORDS;

  function automatic [W-1:0] fill();
    reg [W-1:0] r;
    for (int i = 0; i < WORDS; i++) r[i*16+:16] = 16'(i * 3 + 1);
    return r;
  endfunction

  localparam [W-1:0] P = fill();

  initial begin
    if (P[15:0] !== 16'd1) $stop;
    if (P[16+:16] !== 16'd4) $stop;
    if (P[(WORDS-1)*16+:16] !== 16'((WORDS - 1) * 3 + 1)) $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
