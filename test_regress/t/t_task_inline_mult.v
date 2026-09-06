// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A task inlined into a task that is itself inlined is written out once per
// call path, so the emitted code grows with the number of paths through the
// call graph rather than with the size of the source. Six levels of a fan-out
// of five is 15,625 paths through eight lines of source.

module t (/*AUTOARG*/
   // Inputs
   clk
   );
   input clk;

   int n;

   task automatic lvl0; n = n + 1; endtask
   task automatic lvl1; lvl0(); lvl0(); lvl0(); lvl0(); lvl0(); endtask
   task automatic lvl2; lvl1(); lvl1(); lvl1(); lvl1(); lvl1(); endtask
   task automatic lvl3; lvl2(); lvl2(); lvl2(); lvl2(); lvl2(); endtask
   task automatic lvl4; lvl3(); lvl3(); lvl3(); lvl3(); lvl3(); endtask
   task automatic lvl5; lvl4(); lvl4(); lvl4(); lvl4(); lvl4(); endtask
   task automatic lvl6; lvl5(); lvl5(); lvl5(); lvl5(); lvl5(); endtask

   initial begin
      n = 0;
      lvl6();
      if (n != 15625) $stop;
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
