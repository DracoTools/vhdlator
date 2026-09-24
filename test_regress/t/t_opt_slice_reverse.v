// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// An assignment of an array past the slice element limit is left whole, and
// the whole copy goes storage position by position. Arrays running opposite
// ways pair from the left, so that copy assigned up[0] from dn[0] instead of
// from dn[299].

module t;
  int up[0:299];
  int dn[299:0];
  int u2[0:3][0:299];
  int d2[0:3][299:0];  // Inner dimension reversed
  int o2[3:0][0:299];  // Outer dimension reversed
  int b2[3:0][299:0];  // Both reversed
  int g2[0:19][0:19];  // 400 elements, each dimension under the limit
  int h2[0:19][0:19];

  initial begin
    for (int i = 0; i < 300; i++) dn[i] = i;
    up = dn;
    if (up[0] != 299 || up[299] != 0) $stop;

    for (int r = 0; r < 4; r++) for (int i = 0; i < 300; i++) d2[r][i] = r * 1000 + i;
    u2 = d2;
    if (u2[1][0] != 1299 || u2[1][299] != 1000) $stop;
    o2 = u2;
    if (o2[3][0] != 299 || o2[0][299] != 3000) $stop;
    b2 = u2;
    if (b2[3][299] != 299 || b2[0][0] != 3000) $stop;

    for (int r = 0; r < 20; r++) for (int i = 0; i < 20; i++) g2[r][i] = r * 20 + i;
    h2 = g2;
    if (h2[19][19] != 399 || h2[3][4] != 64) $stop;

    if (up != dn || u2 != d2 || o2 != u2 || b2 != u2 || h2 != g2) $stop;
    up[5] = -1;
    if (up == dn) $stop;

    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
