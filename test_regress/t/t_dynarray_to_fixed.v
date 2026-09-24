// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A dynamic array or queue is assignment compatible with a fixed-size array
// of as many elements, the first going leftmost (IEEE 1800-2023 7.6). The
// assignment was expanded element by element from something with no fixed
// elements, and refused as not an unpacked array.

// verilog_format: off
`define stop $stop
`define checkh(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got='h%x exp='h%x\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0)
// verilog_format: on

module t;
  int d[];
  int q[$];
  int up[0:3];
  int dn[3:0];
  int big[0:299];
  byte dd[][1:0];
  byte f2[0:2][1:0];
  string sd[];
  string sf[2];

  function automatic int digits(int a[0:3]);
    return a[0] + a[1] * 10 + a[2] * 100 + a[3] * 1000;
  endfunction

  initial begin
    d = new[4];
    foreach (d[i]) d[i] = i + 1;
    up = d;
    `checkh(up[0], 1);
    `checkh(up[3], 4);
    dn = d;
    `checkh(dn[3], 1);
    `checkh(dn[0], 4);
    q = {7, 8, 9, 6};
    up = q;
    `checkh(up[0], 7);
    `checkh(up[3], 6);
    `checkh(digits(d), 4321);  // As a function argument

    d = new[300];
    foreach (d[i]) d[i] = 300 - i;
    big = d;  // Past the slice element limit
    `checkh(big[0], 300);
    `checkh(big[299], 1);

    dd = new[3];
    dd[0] = '{101, 100};
    dd[1] = '{111, 110};
    dd[2] = '{121, 120};
    f2 = dd;
    `checkh(f2[0][0], 100);
    `checkh(f2[2][1], 121);

    sd = new[2];
    sd[0] = "a";
    sd[1] = "bc";
    sf = sd;
    if (sf[0] != "a" || sf[1] != "bc") $stop;

    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
