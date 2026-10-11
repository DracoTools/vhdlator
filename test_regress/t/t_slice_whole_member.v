// DESCRIPTION: Verilator: Verilog Test module
//
// Slices of a structure's array member past --fslice-element-limit: all of
// it, which is copied whole, and a part of it, which is expanded
//
// This file ONLY is placed under the Creative Commons Public Domain
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

`define stop $stop
`define checkd(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got=%0d exp=%0d\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0);

module t;
  typedef struct {
    int a[0:599];
    int n;
  } s_t;
  s_t s;
  int x[0:599];
  int y[0:299];

  initial begin
    for (int i = 0; i < 600; i++) s.a[i] = i;
    x = s.a[0:599];
    `checkd(x[0], 0);
    `checkd(x[599], 599);
    y = s.a[100:399];
    `checkd(y[0], 100);
    `checkd(y[299], 399);
    s.a[300:599] = y;
    `checkd(s.a[300], 100);
    `checkd(s.a[599], 399);
    `checkd(s.a[299], 299);
    s.a[0:599] = x;
    `checkd(s.a[300], 300);
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
