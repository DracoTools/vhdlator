// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// The sensed expression is echoed into a C++ string literal to describe the
// trigger, so a string literal inside it has to survive being quoted.

module t;
   string s = "one";
   string bs = "a";

   initial begin
      #10 s = "16 characters OK";
      #10 bs = "back\\slash";
   end

   initial begin
      wait (s == "16 characters OK");
      if ($time != 10) $stop;
      wait (bs == "back\\slash");
      if ($time != 20) $stop;
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
