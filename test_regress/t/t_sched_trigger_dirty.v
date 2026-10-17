// DESCRIPTION: Verilator: Verilog Test module
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of either the GNU Lesser General Public License Version 3
// or the Perl Artistic License Version 2.0.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

// Triggers are computed only where an input of theirs was written: every way
// of writing one must flag it.

`define stop $stop
`define checkd(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got=%0d exp=%0d\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0);

module t;
  int a = 0, b = 0, c = 0, e = 0;
  int arr[4];
  int na = 0, nb = 0, nc = 0, ne = 0, narr = 0;

  always @(a) if ($time > 0) na++;
  always @(b) if ($time > 0) nb++;
  always @(c) if ($time > 0) nc++;
  always @(e) if ($time > 0) ne++;
  always @(arr[2]) if ($time > 0) narr++;

  // A task that is not inlined writes through its output argument
  task automatic set_out(output int o, input int v);
    // verilator no_inline_task
    o = v;
  endtask

  export "DPI-C" function dpi_set;
  function void dpi_set(input int v);
    e = v;
  endfunction
  import "DPI-C" context function void call_dpi_set(input int v);

  initial begin
    #1 set_out(a, 1);
    #1 b = 1;
    #1 b = 2;
    fork
      #1 c = 1;
    join_none
    #2 arr[2] = 5;
    #1 arr[1] = 7;
    #1 call_dpi_set(3);
    #1;
    `checkd(na, 1);
    `checkd(nb, 2);
    `checkd(nc, 1);
    `checkd(narr, 1);
    `checkd(ne, 1);
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
