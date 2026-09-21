// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain, for
// any use, without warranty, 2026 by Wilson Snyder.
// SPDX-License-Identifier: CC0-1.0

module t;
   logic [3:0] v;
   logic [1:0] a, b;
   int         errors = 0;

   initial begin
      v = '0;
      a = '0;
      b = '0;
      // Two parts of one variable, each with its own delay. These must not be
      // merged into a single assignment of the whole variable: the second
      // would then be delivered at the first one's time.
      v[1:0] <= #10 2'b10;
      v[3:2] <= #20 2'b11;
      // The same shape over two separate variables, for contrast
      a <= #10 2'b10;
      b <= #20 2'b11;

      #15;  // past the first delay, before the second
      if (v[1:0] !== 2'b10) begin $display("%%Error: v[1:0]=%b at 15", v[1:0]); errors++; end
      if (v[3:2] !== 2'b00) begin $display("%%Error: v[3:2]=%b at 15, early", v[3:2]); errors++; end
      if (a !== 2'b10) begin $display("%%Error: a=%b at 15", a); errors++; end
      if (b !== 2'b00) begin $display("%%Error: b=%b at 15, early", b); errors++; end

      #10;  // past the second
      if (v[3:2] !== 2'b11) begin $display("%%Error: v[3:2]=%b at 25", v[3:2]); errors++; end
      if (b !== 2'b11) begin $display("%%Error: b=%b at 25", b); errors++; end

      if (errors == 0) $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
