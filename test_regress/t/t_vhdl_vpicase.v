// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A Verilog module a VHDL architecture instantiates. Its names differ only
// in case, so they must stay distinct over VPI.
module t_vhdl_vpicase_v (
    output logic o
);
  logic Foo = 1'b1;
  logic foo = 1'b0;
  assign o = Foo ^ foo;
endmodule
