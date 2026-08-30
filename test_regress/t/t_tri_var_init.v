// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain, for
// any use, without warranty, 2026 by Wilson Snyder.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A variable's initial value is what it holds before anything drives it,
// not a driver of it.  A 'z' there must not turn the variable into a
// resolved net, whose continuous drivers would overwrite what a procedural
// assignment -- here one made inside a task -- has written to it.

module t;
   logic [3:0] s = 4'bzzzz;
   logic [3:0] v = 4'bzzzz;
   wire  [3:0] w = 4'b11z1;

   task automatic drv(); s[3] = 1'b1; endtask

   initial begin
      drv();
      v[0] = 1'b1;
      #1;
      if (s[3] !== 1'b1) begin
         $write("Error: s = %b, expected s[3] = 1\n", s);
         $stop;
      end
      if (v[0] !== 1'b1) begin
         $write("Error: v = %b, expected v[0] = 1\n", v);
         $stop;
      end
      // A net's initializer is a continuous assignment, and a 'z' in one
      // does make it a driver -- that must go on working
      if (w[1] !== 1'bz) begin
         $write("Error: w = %b, expected w[1] = z\n", w);
         $stop;
      end
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
