// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// Two pieces of logic that compute the same thing are duplicates only if they
// compute it at the same times. The four instances below share a right-hand
// side and nothing else: each is clocked separately, so none may stand for
// another. The pair at the bottom is clocked together, and is one duplicate.

module t(/*AUTOARG*/
  // Outputs
  o,
  // Inputs
  clk, en, a, b
  );
  input clk;
  input [3:0] en;
  input a;
  input b;
  output [4:0] o;

  sub s0 (.clk(en[0]), .a(a), .b(b), .q(o[0]));
  sub s1 (.clk(en[1]), .a(a), .b(b), .q(o[1]));
  sub s2 (.clk(en[2]), .a(a), .b(b), .q(o[2]));
  sub s3 (.clk(en[3]), .a(a), .b(b), .q(o[3]));

  reg d0, d1;
  always @(posedge clk) d0 <= a & b;
  always @(posedge clk) d1 <= a & b;
  assign o[4] = d0 ^ d1;
endmodule

module sub(/*AUTOARG*/
  // Outputs
  q,
  // Inputs
  clk, a, b
  );
  input clk;
  input a;
  input b;
  output reg q;
  always @(posedge clk) q <= a & b;
endmodule
