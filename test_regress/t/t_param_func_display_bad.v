// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

module t;
   function automatic int f(int x);
      // $time is not constant, so the format has no value to fetch and the
      // function cannot be evaluated at elaboration
      $display("f called at %0t with %0d", $time, x);
      return x + 1;
   endfunction
   localparam int P = f(1);
   initial begin
      if (P != 2) $stop;
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
