// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A process sensitive to value changes that reads the simulation time must
// rerun on every change of its sensitivity list, even though the body reads
// no variable of that list

`define stop $stop
`define checkd(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got=%0d exp=%0d\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0);

module t (
    input clk
);

  logic s = 0;
  logic r = 0;
  logic y;
  time te;
  time tb;
  realtime tr;
  time ts;
  time tq;
  int cyc = 0;

  always @(s) te = $time;
  always @(s or r) tr = $realtime;
  always @(s) begin
    tb = $time;
    y = s;
  end

  always @(posedge clk) begin
    cyc <= cyc + 1;
    if (cyc == 3) begin
      s <= 1;
      ts <= $time;
    end
    if (cyc == 5) begin
      r <= 1;
      tq <= $time;
    end
    if (cyc == 8) begin
      `checkd(ts != 0, 1);
      `checkd(te, ts);
      `checkd(tb, ts);
      `checkd(y, 1'b1);
      `checkd(tr == $itor(tq), 1);
      $write("*-* All Finished *-*\n");
      $finish;
    end
  end

endmodule
