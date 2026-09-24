// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// Comparisons of arrays over the slice element limit: each one expanded
// to a term per element made megabytes of C++ for a 1024 by 64 array

module t;
  typedef struct packed {
    bit [3:0] a;
    bit [11:0] b;
  } ps_t;
  typedef struct {
    int a;
    real b;
  } us_t;
  typedef struct {int v[300];} box_t;

  int ai[300], bi[300];
  bit [99:0] aw[300], bw[300];
  real ar[300], br[300];
  string as[300], bs[300];
  ps_t ap[300], bp[300];
  us_t au[300], bu[300];
  int a2[20][20], b2[20][20];

  // Opposite directions pair the elements from the left: the outer one by a
  // reversed copy, the inner ones element by element
  int up[0:299];
  int dn[299:0];
  int u2[0:19][0:19];
  int d2[0:19][19:0];

  box_t bx1, bx2;
  int cube1[2][300], cube2[2][300];
  int calls = 0;
  bit same;

  function automatic int pick();
    calls++;
    return 1;
  endfunction

  initial begin
    if (!(ai == bi) || (ai != bi)) $stop;
    if (!(aw == bw) || (aw != bw)) $stop;
    if (!(ar == br) || (ar != br)) $stop;
    if (!(as == bs) || (as != bs)) $stop;
    if (!(ap == bp) || (ap != bp)) $stop;
    if (!(au == bu) || (au != bu)) $stop;
    if (!(a2 == b2) || (a2 != b2)) $stop;
    ai[299] = 1;
    aw[150][99] = 1'b1;
    ar[0] = 0.5;
    as[7] = "x";
    ap[42].b = 12'h5;
    au[3].b = 2.5;
    a2[19][19] = 9;
    if ((ai == bi) || !(ai != bi)) $stop;
    if ((aw == bw) || !(aw != bw)) $stop;
    if ((ar == br) || !(ar != br)) $stop;
    if ((as == bs) || !(as != bs)) $stop;
    if ((ap == bp) || !(ap != bp)) $stop;
    if ((au == bu) || !(au != bu)) $stop;
    if ((a2 == b2) || !(a2 != b2)) $stop;

    for (int i = 0; i < 300; i++) begin
      up[i] = i;
      dn[299-i] = i;
    end
    if (!(up == dn) || (up != dn)) $stop;
    for (int i = 0; i < 20; i++) begin
      for (int j = 0; j < 20; j++) begin
        u2[i][j] = i * 20 + j;
        d2[i][19-j] = i * 20 + j;
      end
    end
    if (!(u2 == d2) || (u2 != d2)) $stop;

    if (!(bx1.v == bx2.v) || (bx1.v !== bx2.v) || !(bx1.v === bx2.v)) $stop;
    bx2.v[123] = 5;
    if ((bx1.v == bx2.v) || !(bx1.v !== bx2.v)) $stop;
    cube1[1][299] = 7;
    if ((cube1[1] == cube2[1]) || !(cube1[0] == cube2[0])) $stop;
    same = cube1[pick()] == cube2[pick()];  // Each index evaluated once
    if (same || calls != 2) $stop;

    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
