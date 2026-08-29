// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain, for
// any use, without warranty, 2026 by Wilson Snyder.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

module t;
   bit x;

   // The wait's condition reads an automatic of the task, which the fork
   // captures into a dynamic scope. The handle of that scope is null until
   // this task reaches the fork, so the trigger cannot be a global one.
   task automatic waiter(input bit value);
      fork
         begin wait (x == value); end
         begin #5; end
      join_any
      disable fork;
   endtask

   initial begin
      waiter(1'b1);
      if ($time != 1) $stop;
      $write("*-* All Finished *-*\n");
      $finish;
   end

   initial begin
      #1;
      x = 1'b1;
   end
endmodule
