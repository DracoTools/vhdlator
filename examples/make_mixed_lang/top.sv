// DESCRIPTION: Verilator Example: the Verilog half of a mixed-language design
//
// This file ONLY is placed under the Creative Commons Public Domain
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// VHDL counts time in femtoseconds, which becomes the design's precision, so
// a Verilog file that means nanoseconds has to say so.
`timescale 1ns/1ps

// The adder the VHDL architecture instantiates ...
module adder #(parameter WIDTH = 8) (
  input  logic [WIDTH-1:0] a,
  input  logic [WIDTH-1:0] b,
  output logic [WIDTH-1:0] s
  );
  assign s = a + b;
endmodule

// ... and the testbench that instantiates the VHDL entity. Binding is
// case-insensitive from the VHDL side, so COUNTER, Counter and counter all
// name the same entity.
module top;
   logic clk = 1'b0;
   logic rst = 1'b1;
   logic [7:0] q;

   COUNTER #(.Width(8)) u_dut (.CLK(clk), .Rst(rst), .Q(q));

   always #5 clk = ~clk;
   initial #22 rst = 1'b0;

   initial begin
      #100;
      $display("count = %0d", q);
      if (q !== 8'd8) begin
         $display("%%Error: expected 8");
         $stop;
      end
      $write("-- VHDL and Verilog in one model. --\n");
      $finish;
   end
endmodule
