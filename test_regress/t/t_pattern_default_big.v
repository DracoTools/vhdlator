// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A default in an assignment pattern is one value for every element the
// pattern does not name. For a memory of millions of words it must stay one
// value: expanding it per element made this module take gigabytes to
// verilate, growing with the size of the array.

module t;
  localparam int WORDS = 1 << 22;
  int mem[0:WORDS-1] = '{default: 255};
  int sparse[0:WORDS-1];

  initial begin
    if (mem[0] !== 255 || mem[WORDS/2] !== 255 || mem[WORDS-1] !== 255) $stop;
    sparse = '{7: 3, WORDS-1: 4, default: -1};
    if (sparse[0] !== -1 || sparse[7] !== 3 || sparse[WORDS-1] !== 4) $stop;
    mem = '{default: 9};
    if (mem[123] !== 9) $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
