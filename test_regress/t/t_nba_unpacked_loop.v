// DESCRIPTION: Verilator: Non-blocking assignment of a whole unpacked array
// in a loop of a process that suspends
//
// As t_nba_unpacked_susp, but inside a loop: a process that never ends is
// written as one, and the shadow variable copied at the commit overwrote
// what the process had written there too. The same NBA run several times
// in one time step commits the value it was given last.
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

module t;
  int s[3:0];
  int tmp[3:0] = '{1, 2, 3, 4};
  int r[0:1];
  int src[0:1];
  int cyc = 0;

  initial begin
    forever begin
      #1;
      s <= tmp;
      for (int i = 0; i < 3; i++) begin
        src = '{i, i * 10};
        r <= src;
      end
      wait (cyc == 2);
    end
  end

  initial begin
    #2;
    if (s[0] !== 4 || s[3] !== 1) $stop;
    if (r[0] !== 2 || r[1] !== 20) $stop;
    tmp = '{5, 6, 7, 8};
    cyc = 1;
    #1;
    // The first process waits, so nothing more is written
    if (s[0] !== 4) $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
