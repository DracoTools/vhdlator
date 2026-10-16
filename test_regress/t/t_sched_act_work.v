// DESCRIPTION: Verilator: Verilog Test module
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of either the GNU Lesser General Public License Version 3
// or the Perl Artistic License Version 2.0.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

// The 'act' loop stops after a pass in which only triggers that later regions
// act on fired. Triggers fired by 'nba' logic must still wake 'act' logic and
// suspended processes.

`define stop $stop
`define checkd(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got=%0d exp=%0d\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0);

module t;
  logic clk = 0;
  always #5 clk = ~clk;

  int cyc = 0;
  logic en = 1;
  // A derived clock: 'act' logic, triggered by 'en', which 'nba' logic writes
  logic gclk;
  always_comb gclk = clk & en;

  int cnt_clk = 0;
  int cnt_gclk = 0;
  always @(posedge clk) begin
    cyc <= cyc + 1;
    cnt_clk <= cnt_clk + 1;
    if (cyc == 5) en <= 0;
    if (cyc == 8) en <= 1;
  end
  always @(posedge gclk) cnt_gclk <= cnt_gclk + 1;

  // A suspended process woken by a variable 'nba' logic writes
  int seen = 0;
  initial forever begin
    @(cyc);
    seen = seen + 1;
  end

  always @(negedge clk) begin
    `checkd(seen, cyc);
    if (cyc == 5) `checkd(cnt_gclk, 5);
    if (cyc == 8) `checkd(cnt_gclk, 6);
    if (cyc == 12) begin
      `checkd(cnt_clk, 12);
      `checkd(cnt_gclk, 10);
      $write("*-* All Finished *-*\n");
      $finish;
    end
  end
endmodule
