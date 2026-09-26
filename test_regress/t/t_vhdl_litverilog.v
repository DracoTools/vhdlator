// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// Instantiated from t_vhdl_litverilog.vhd
module t_vhdl_litverilog_sub #(
    parameter logic INV = 1'b0
) (
    input logic x,
    input logic en,
    output logic y
);
  assign y = en ? (x ^ INV) : 1'b0;
endmodule
