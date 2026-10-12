// DESCRIPTION: Verilator: Verilog Test module
//
// An inout port that is an unpacked array of vectors, driven an element at
// a time. Tristate resolution builds bitwise operations over the whole
// unpacked array, which are taken an element at a time like an assignment.
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

module sub (
    inout wire [1:0] io_a[1:0]
);
  assign io_a[1] = 2'b11;
  assign io_a[0] = 2'b01;
endmodule

module t;
  wire [1:0] w[1:0];
  sub u (.io_a(w));
  initial begin
    #1;
    if (w[1] !== 2'b11) $stop;
    if (w[0] !== 2'b01) $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
