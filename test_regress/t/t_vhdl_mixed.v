// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// The Verilog half: a module a VHDL architecture instantiates, and a
// testbench that instantiates the VHDL entity. Names are spelled in a
// different case on each side of the boundary, which VHDL allows.
module adder #(parameter WIDTH = 8) (
  input  logic [WIDTH-1:0] a,
  input  logic [WIDTH-1:0] b,
  output logic [WIDTH-1:0] s
  );
  assign s = a + b;
endmodule

module t (
  input  logic       clk,
  input  logic       rst,
  output logic [7:0] sum
  );
  T_VHDL_MIXED #(.Width(8)) u_dut (.CLK(clk), .Rst(rst), .SUM(sum));
endmodule
