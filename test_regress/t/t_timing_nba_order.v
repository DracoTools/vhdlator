// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// Nonblocking assignments to one variable from two processes that suspend,
// in one time slot: they are performed in the order they were executed
// (IEEE 1800-2023 10.4.2), so the last one to run decides. A variable
// assigned this way kept one value per assignment in the source and
// committed them in source order, so the one written first in the source
// won whichever ran last.

// verilog_format: off
`define stop $stop
`define checkd(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got=%0d exp=%0d\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0)
// verilog_format: on

module t;
  int x = 0;
  int y = 0;
  logic [7:0] z = 0;
  logic trig = 0;

  // Written first, run second: after the #0
  initial begin
    #1;
    #0;
    x <= 2;
  end
  initial begin
    #1;
    x <= 1;
  end

  // The same, over and over, with the later one reading what it adds to
  initial begin
    repeat (3) begin
      #10;
      #0;
      y <= y + 100;
    end
  end
  initial begin
    repeat (3) begin
      #10;
      y <= 7;
    end
  end

  // Parts of a vector: each bit is the last one written to it
  initial begin
    #50;
    #0;
    z[3:0] <= 4'ha;
  end
  initial begin
    #50;
    z <= 8'hff;
  end

  // A pulse two processes raise and lower together: VHDL's delta cycles
  // written out, as a package signal every VHDL process can drive is
  initial begin
    #60;
    trig <= 1'b1;
    #0;
    trig <= 1'b0;
  end
  initial begin
    #60;
    #0;
    trig <= 1'b1;
    #0;
    #0;
    trig <= 1'b0;
  end

  initial begin
    #2 `checkd(x, 2);
    #38 `checkd(y, 300);
    #20 `checkd(z, 8'hfa);
    #10 `checkd(trig, 1'b0);
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
