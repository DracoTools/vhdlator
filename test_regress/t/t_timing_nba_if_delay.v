// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// The two branches of an if assign the same variable from the same value,
// but with different intra-assignment delays: only the branch taken decides
// when the value arrives. Folding the if into one assignment kept the first
// branch's delay whatever the condition said.

// verilator lint_off ZERODLY
module t;
  logic [7:0] i = 0;
  logic [7:0] o = 0;
  logic [7:0] b = 0;
  time d_short;
  time d_long;
  int kind;

  initial begin
    d_short = 0ns;
    d_long = 7ns;
    kind = 1;
    forever begin
      @(i);
      if (kind == 3) o <= #(d_short) i;
      else o <= #(d_long) i;
      if (kind != 3) b = #(d_short) i;
      else b = #(d_long) i;
    end
  end

  initial begin
    #1ns i = 8'h01;
    #1ns;
    if (o !== 8'h00) $stop;
    if (b !== 8'h01) $stop;
    #7ns;
    if (o !== 8'h01) $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
