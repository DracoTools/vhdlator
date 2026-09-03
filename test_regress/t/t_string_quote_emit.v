// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain, for
// any use, without warranty, 2026 by Wilson Snyder.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A string constant containing a quote is emitted with that quote escaped.
// The output formatter counts braces and parentheses to indent what it
// writes, and has to know which of them are inside a string literal.

module t;
   string s;
   initial begin
      if ($time == 0) s = "a\"b"; else s = "z";
      if (s != "a\"b") $stop;
      if ($time == 0) s = "))))))))))))))))))))\"x"; else s = "z";
      if (s != "))))))))))))))))))))\"x") $stop;
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
