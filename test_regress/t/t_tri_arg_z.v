// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A high impedance constant handed to a subprogram is a value, not a driver
// of a net: there is nothing inside a subprogram for it to resolve against.
// So it is worth exactly what the same constant put in a variable is worth.

module t;

  logic q;
  logic [3:0] w;

  function automatic logic f(logic v);
    return v;
  endfunction

  function automatic logic [3:0] g(logic [3:0] v);
    return v;
  endfunction

  initial begin
    q = 1'bz;
    w = 4'b01z0;
    if (f(1'bz) !== f(q)) $stop;
    if (f(1'bz) !== q) $stop;
    if (g(4'b01z0) !== g(w)) $stop;
    if (g(4'b01z0) !== w) $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
