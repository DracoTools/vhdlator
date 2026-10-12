// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

module t (
    input en,
    output y
);
  // A 'z' in the left operand of ==? makes the result 'x', which is not
  // modelled
  assign y = ((en ? 2'b01 : 2'bzz) ==? 2'b0x);
endmodule
