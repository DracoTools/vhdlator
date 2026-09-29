// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// Nonblocking assignments with intra-assignment delays, one of them high
// impedance: the process is one driver of the variable, and each value it
// hands over replaces the last one, the z one releasing it

module t;
  logic [1:0] value;
  initial begin
    value <= 2'b10;
    value <= #3 2'b01;
    value <= #5 2'bzz;
    #1;
    if (value !== 2'b10) begin
      $write("%%Error: %s:%0d: value=%b\n", `__FILE__, `__LINE__, value);
      $stop;
    end
    #3;
    if (value !== 2'b01) begin
      $write("%%Error: %s:%0d: value=%b\n", `__FILE__, `__LINE__, value);
      $stop;
    end
    #2;
    if (value !== 2'bzz) begin
      $write("%%Error: %s:%0d: value=%b\n", `__FILE__, `__LINE__, value);
      $stop;
    end
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
