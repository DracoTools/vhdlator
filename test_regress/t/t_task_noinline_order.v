// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A task left out of line is still a statement of whoever calls it, so what it
// reads and writes has to be ordered with the caller. Here every access the
// combinational logic makes is inside the calls, and nothing outside them says
// which signals the block depends on.

module t (/*AUTOARG*/
  // Inputs
  clk
  );
  input clk;

  logic [7:0] a;
  logic [7:0] b;
  logic [7:0] sum;
  logic [7:0] prod;
  int         cyc = 0;

  function automatic logic [7:0] readsA();
    /* verilator no_inline_task */
    return a + 8'd1;
  endfunction

  function automatic logic [7:0] readsB();
    /* verilator no_inline_task */
    return b + 8'd2;
  endfunction

  task automatic writesProd();
    /* verilator no_inline_task */
    prod = readsA() * readsB();
  endtask

  always_comb sum = readsA() + readsB();
  always_comb writesProd();

  always @(posedge clk) begin
    cyc <= cyc + 1;
    case (cyc)
      0: begin a <= 8'd10; b <= 8'd20; end
      1: begin
        if (sum !== 8'd33) $stop;  // 11 + 22
        if (prod !== 8'd242) $stop;  // 11 * 22
        a <= 8'd3;
      end
      2: begin
        if (sum !== 8'd26) $stop;  // 4 + 22
        if (prod !== 8'd88) $stop;  // 4 * 22
        b <= 8'd5;
      end
      3: begin
        if (sum !== 8'd11) $stop;  // 4 + 7
        if (prod !== 8'd28) $stop;  // 4 * 7
      end
      4: begin
        $write("*-* All Finished *-*\n");
        $finish;
      end
    endcase
  end
endmodule
