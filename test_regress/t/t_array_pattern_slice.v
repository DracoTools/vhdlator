// DESCRIPTION: Verilator: Verilog Test module
//
// A slice of an unpacked array as an item of an assignment pattern for an
// array of arrays: the slice is one element, not a run of scalars.
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

module t;
  int a[1:4][1:2];
  int b[1:8];
  int c[7:0];
  int d[3:0][1:0];
  int e[0:1][0:2];

  initial begin
    b = '{1, 2, 3, 4, 5, 6, 7, 8};
    a = '{b[1:2], b[3:4], b[5:6], b[7:8]};
    if (a[1][1] != 1 || a[1][2] != 2) $stop;
    if (a[2][1] != 3 || a[2][2] != 4) $stop;
    if (a[3][1] != 5 || a[3][2] != 6) $stop;
    if (a[4][1] != 7 || a[4][2] != 8) $stop;

    // Ranges numbered differently from the element's own
    c = '{17, 16, 15, 14, 13, 12, 11, 10};
    d = '{c[7:6], c[5:4], c[3:2], c[1:0]};
    if (d[3][1] != 17 || d[3][0] != 16) $stop;
    if (d[2][1] != 15 || d[2][0] != 14) $stop;
    if (d[0][1] != 11 || d[0][0] != 10) $stop;

    e = '{b[2:4], b[6:8]};
    if (e[0][0] != 2 || e[0][1] != 3 || e[0][2] != 4) $stop;
    if (e[1][0] != 6 || e[1][1] != 7 || e[1][2] != 8) $stop;

    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
