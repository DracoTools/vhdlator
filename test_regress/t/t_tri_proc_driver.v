// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// Several procedural assignments of one process to a variable that is an
// inout's actual are one driver: a later assignment replaces what an
// earlier one drove. Each statement was a driver of its own whose enable
// was fixed for ever, so "s <= 2'b0z" after "s[1] <= 1" left bit 1 driven
// high by the earlier statement.

module t_tri_proc_driver_sink (
    inout logic [1:0] ifc
);
  initial ifc <= 2'bz1;
endmodule

module t;
  logic [1:0] s;
  t_tri_proc_driver_sink u (.ifc(s));

  initial begin
    s <= 2'b0z;
    #5;
    if (s !== 2'b01) begin
      $display("%%Error: first s=%b", s);
      $stop;
    end
    s[1] <= 1'b1;
    #5;
    if (s !== 2'b11) begin
      $display("%%Error: after bit s=%b", s);
      $stop;
    end
    s <= 2'b0z;
    #5;
    if (s !== 2'b01) begin
      $display("%%Error: after whole s=%b", s);
      $stop;
    end
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
