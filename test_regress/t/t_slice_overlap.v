// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// An assignment is evaluated in full before any of it is assigned, so an
// unpacked array assignment whose right-hand side reads the array it writes
// must see the old values throughout.

module t;

  int v [1:3];
  int w [0:3];
  int u [3:0];
  int two [0:1];
  int dyn [][1:3];

  initial begin
    // Reverse. Each element read must be the value before the assignment.
    v = '{1, 5, 2};
    v = '{v[3], v[2], v[1]};
    if (v[1] != 2) $stop;
    if (v[2] != 5) $stop;
    if (v[3] != 1) $stop;

    // Swap, which no ordering of element assignments alone can do
    two = '{7, 9};
    two = '{two[1], two[0]};
    if (two[0] != 9) $stop;
    if (two[1] != 7) $stop;

    // Rotate through an assignment pattern
    w = '{10, 20, 30, 40};
    w = '{w[3], w[0], w[1], w[2]};
    if (w[0] != 40) $stop;
    if (w[1] != 10) $stop;
    if (w[2] != 20) $stop;
    if (w[3] != 30) $stop;

    // Overlapping slice assignment, shifting up
    w = '{10, 20, 30, 40};
    w[1:3] = w[0:2];
    if (w[0] != 10) $stop;
    if (w[1] != 10) $stop;
    if (w[2] != 20) $stop;
    if (w[3] != 30) $stop;

    // Overlapping slice assignment, shifting down
    w = '{10, 20, 30, 40};
    w[0:2] = w[1:3];
    if (w[0] != 20) $stop;
    if (w[1] != 30) $stop;
    if (w[2] != 40) $stop;
    if (w[3] != 40) $stop;

    // A descending range behaves the same way
    u = '{1, 2, 3, 4};
    u = '{u[0], u[1], u[2], u[3]};
    if (u[3] != 4) $stop;
    if (u[2] != 3) $stop;
    if (u[1] != 2) $stop;
    if (u[0] != 1) $stop;

    // An element of a dynamic array, reached through a method rather than a
    // select, must behave the same way
    dyn = new[2];
    dyn[0] = '{1, 5, 2};
    dyn[0] = '{dyn[0][3], dyn[0][2], dyn[0][1]};
    if (dyn[0][1] != 2) $stop;
    if (dyn[0][2] != 5) $stop;
    if (dyn[0][3] != 1) $stop;

    // No overlap: the ordinary case must be unaffected
    v = '{1, 2, 3};
    w = '{v[1], v[2], v[3], 0};
    if (w[0] != 1) $stop;
    if (w[1] != 2) $stop;
    if (w[2] != 3) $stop;
    if (w[3] != 0) $stop;

    $write("*-* All Finished *-*\n");
    $finish;
  end

endmodule
