// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain, for
// any use, without warranty, 2026 by Wilson Snyder.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

module t;

   // A task that waits on a condition that is never true never returns, so
   // neither does its caller, however deep the call stack is
   task automatic hangs(int id);
      /* verilator no_inline_task */
      $write("[%0t] hangs %0d\n", $time, id);
      wait (0);
   endtask

   task automatic outer(int id);
      /* verilator no_inline_task */
      hangs(id);
      $write("%%Error: returned from a task that waits forever\n");
      $stop;
   endtask

   initial begin
      outer(1);
      $write("%%Error: returned from a task that waits forever\n");
      $stop;
   end

   // A second caller, so the one that hangs is not the only process running
   initial begin
      #5;
      hangs(2);
      $write("%%Error: returned from a task that waits forever\n");
      $stop;
   end

   initial begin
      #10;
      $write("*-* All Finished *-*\n");
      $finish;
   end

endmodule
