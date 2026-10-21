// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A nonblocking assignment made in a task that is not inlined, by a process the
// clock edge resumed, is not seen by an always block the same edge triggers: that
// block runs in the same Active region and reads the value from before the NBA
// region (IEEE 1800-2023 4.4.2.2, 10.4.2). Nor is it seen through a continuous
// assignment there; on the next edge both see it.

`define stop $stop
`define checkd(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got=%0d exp=%0d\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0);

module t;
  logic clk = 1;
  int req = 5;
  int both;
  int cyc = 0;
  assign both = req + 100;
  always #5 clk = ~clk;

  task automatic set_req(int v);
    /*verilator no_inline_task*/
    req <= v;
    @(posedge clk);
  endtask

  initial begin
    @(posedge clk);
    set_req(0);
    set_req(7);
    set_req(9);
  end

  always @(posedge clk) begin
    cyc <= cyc + 1;
    case (cyc)
      0: begin `checkd(req, 5); `checkd(both, 105); end
      1: begin `checkd(req, 0); `checkd(both, 100); end
      2: begin `checkd(req, 7); `checkd(both, 107); end
      3: begin
        `checkd(req, 9);
        `checkd(both, 109);
        $write("*-* All Finished *-*\n");
        $finish;
      end
      default: ;
    endcase
  end
endmodule
