// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain, for
// any use, without warranty, 2026 by Wilson Snyder.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// What a blocking assignment writes has to be there for the statement that
// follows it to read.  Resolution cannot give it that -- resolution is a
// continuous assignment, which runs after the process rather than between
// two of its statements -- so a variable a blocking assignment gives 'z'
// must stay an ordinary variable and keep every write made to it.

module t;
   logic       ok;
   logic [3:0] v;
   int         errs = 0;

   task automatic drv(); ok = 1'b1; endtask

   initial begin
      ok = 1'b1;
      if (ok !== 1'b1) begin errs++; $display("%%Error: first write lost"); end
      ok = 1'b0;
      if (ok !== 1'b0) begin errs++; $display("%%Error: second write lost"); end
      ok = 1'b1;
      if (ok !== 1'b1) begin errs++; $display("%%Error: third write lost"); end
      drv();
      if (ok !== 1'b1) begin errs++; $display("%%Error: write from a task lost"); end
      // The 'z' that used to lose every write above it
      ok = 1'bz;

      v = 4'b1010;
      if (v !== 4'b1010) begin errs++; $display("%%Error: vector write lost"); end
      v[3] = 1'b0;
      if (v !== 4'b0010) begin errs++; $display("%%Error: part write lost"); end
      v = 4'bzzzz;

      if (errs == 0) $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
