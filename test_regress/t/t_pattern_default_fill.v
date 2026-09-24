// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// An initializer that is only a default, for an array too long to write out
// element by element, where it is not a whole variable assigned: written as
// a list, nine 1024 by 64 members of one record were 22 MB of C++

module t;
  typedef struct {
    int e[300];
    string s[300];
    logic [99:0] w[300];
    int few[4];
  } r_t;

  r_t x;
  int m[2][300];

  initial begin
    x = '{e: '{default: 7}, s: '{default: "ab"}, w: '{default: 100'h3}, few: '{default: 2}};
    if (x.e[0] != 7 || x.e[299] != 7) $stop;
    if (x.s[0] != "ab" || x.s[150] != "ab") $stop;
    if (x.w[0] != 100'h3 || x.w[299] != 100'h3) $stop;
    if (x.few[3] != 2) $stop;
    x.e = '{default: 3};
    if (x.e[0] != 3 || x.e[299] != 3 || x.s[299] != "ab") $stop;
    x.s = '{5: "five", default: "cd"};  // Names an element, so written out
    if (x.s[5] != "five" || x.s[6] != "cd") $stop;
    m[1] = '{default: 4};
    if (m[1][0] != 4 || m[1][299] != 4 || m[0][0] != 0) $stop;
    m[0] = '{default: -1};
    if (m[0][150] != -1 || m[1][150] != 4) $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
