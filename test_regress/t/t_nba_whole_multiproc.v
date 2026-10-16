// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// Two processes assign a whole unpacked array, or a whole unpacked
// structure, by nonblocking assignment in one time slot: the one executed
// last is the one committed (IEEE 1800-2023 10.4.2), whichever comes first
// in the source

module t;
  typedef struct {
    int a;
    int b;
  } s_t;
  int x[0:1];
  int y[0:1];
  s_t r;

  // Written before the initial below, which runs first and reads x before
  // it is set; this one runs after, and its value is the one left
  always @(x[0], x[1]) y <= x;
  initial y <= x;
  initial x = '{5, 5};

  // Written first, but runs after the #0
  initial begin
    #0;
    r <= '{a: 2, b: 2};
  end
  initial r <= '{a: 1, b: 1};

  initial begin
    #1;
    if (y[0] != 5 || y[1] != 5) begin
      $display("%%Error: y=%0d %0d", y[0], y[1]);
      $stop;
    end
    if (r.a != 2 || r.b != 2) begin
      $display("%%Error: r=%0d %0d", r.a, r.b);
      $stop;
    end
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
