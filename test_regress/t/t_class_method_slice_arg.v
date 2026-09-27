// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A slice of an unpacked array given to an input of a class method: the call
// is kept out of line, so the slice was handed to the emitter as it was, and
// the emitter stopped on an internal error. An inlined call's input goes
// through a temporary, which is where a slice is expanded.

// verilog_format: off
`define stop $stop
`define checkd(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got=%0d exp=%0d\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0)
// verilog_format: on

class Gen;
  function int sum(int v[35]);
    int s = 0;
    foreach (v[i]) s += v[i];
    return s;
  endfunction
  static function int first(int v[3]);
    return v[0];
  endfunction
  task add_to(int v[4], output int s);
    s = 0;
    foreach (v[i]) s += v[i];
  endtask
endclass

module t;
  int a[1:256];
  int d[8:1];
  Gen g;
  int s;

  initial begin
    g = new;
    foreach (a[i]) a[i] = i;
    foreach (d[i]) d[i] = 10 * i;
    `checkd(g.sum(a[1:35]), 630);
    `checkd(g.sum(a[100:134]), 4095);
    `checkd(Gen::first(a[7:9]), 7);
    // A descending range, taken from its left
    `checkd(Gen::first(d[6:4]), 60);
    g.add_to(a[2:5], s);
    `checkd(s, 14);
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
