// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// Names looked up over VPI relative to a scope handle, in a model with the
// default name, whose scopes are kept as TOP.<path> while a handle's
// vpiFullName leaves the TOP. out.

module sub (
    input clk
);
  logic [3:0] x = 4'd5;
endmodule

module t (
    input clk
);
  logic [7:0] y = 8'd9;
  sub u_sub (.clk(clk));
endmodule
