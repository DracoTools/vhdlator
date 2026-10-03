// DESCRIPTION: Verilator: Non-blocking assignment of a whole unpacked array
// in a process that suspends
//
// The whole array is the target, so it used a shadow variable copied from
// the array before the process ran; a process that suspends runs at no such
// moment, and the copy, taken at the commit, overwrote what it had written.
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

module t;
   int s[1:5];
   int tmp[1:5] = '{1, 2, 3, 4, 5};
   int r[0:1];

   always begin
      s <= tmp;
      #1;
      if (s[3] !== 3) $stop;
      if (s[5] !== 5) $stop;
      for (int i = 1; i <= 5; i++) tmp[i] = i * 10;
      s <= tmp;
      // Not yet: the value lands at the end of the time step
      if (s[3] !== 3) $stop;
      #1;
      if (s[3] !== 30) $stop;
      // Two in one time step: the last one written is the one committed
      r <= '{1, 2};
      r <= '{3, 4};
      #1;
      if (r[0] !== 3 || r[1] !== 4) $stop;
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
