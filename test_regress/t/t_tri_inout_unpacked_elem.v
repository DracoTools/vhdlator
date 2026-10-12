// DESCRIPTION: Verilator: Verilog Test module
//
// An inout port that is an unpacked array, one element of it driven with
// high impedance by turns from two modules, the other driven throughout.
// Each driver drives only its element; where neither does, the element is z.
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

module b (
    inout wire [1:0] io_a[1:0],
    input en,
    output [1:0] seen
);
  assign io_a[1] = en ? 2'b11 : 2'bzz;
  assign io_a[0] = 2'b01;
  assign seen = io_a[1];
endmodule

module c (
    inout wire [1:0] io_a[1:0],
    input en
);
  assign io_a[1] = en ? 2'b10 : 2'bzz;
endmodule

module t;
  wire [1:0] w[1:0];
  wire [1:0] seen;
  reg en_b;
  reg en_c;
  b u_b (
      .io_a(w),
      .en(en_b),
      .seen(seen)
  );
  c u_c (
      .io_a(w),
      .en(en_c)
  );
  initial begin
    en_b = 1;
    en_c = 0;
    #1;
    if (w[1] !== 2'b11 || w[0] !== 2'b01 || seen !== 2'b11) $stop;
    en_b = 0;
    en_c = 1;
    #1;
    if (w[1] !== 2'b10 || w[0] !== 2'b01 || seen !== 2'b10) $stop;
    // Neither drives the element: it floats, and the other is unchanged
    en_c = 0;
    #1;
    if (w[1] !== 2'bzz) $stop;
    if (w[0] !== 2'b01) $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
