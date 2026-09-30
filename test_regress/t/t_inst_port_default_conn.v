// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A default value on an input port applies only where the port is left
// unconnected (IEEE 1800-2023 23.2.2.4). Connected to a variable that is
// never assigned, the port must read that variable's initial value, not
// the default -- whether or not the instance is inlined.

`define stop $stop
`define checkd(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got=%0d exp=%0d\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0);

module sub (
    input logic d = 1'b1,
    input logic u = 1'b1,
    output logic q,
    output logic uq
);
  assign q = d;
  assign uq = u;
endmodule

module sub_noinl (
    input logic d = 1'b1,
    output logic q
);
  /*verilator no_inline_module*/
  assign q = d;
endmodule

module t (
    input clk
);

  logic zero = 1'b0;  // Never assigned
  logic q_inl, uq_inl, q_noinl;
  int cyc = 0;

  sub u_inl (
      .d(zero),
      .q(q_inl),
      .uq(uq_inl)
  );
  sub_noinl u_noinl (
      .d(zero),
      .q(q_noinl)
  );

  always @(posedge clk) begin
    cyc <= cyc + 1;
    if (cyc == 2) begin
      `checkd(zero, 1'b0);
      `checkd(q_inl, 1'b0);
      `checkd(q_noinl, 1'b0);
      // Left unconnected, so the default applies
      `checkd(uq_inl, 1'b1);
      $write("*-* All Finished *-*\n");
      $finish;
    end
  end

endmodule
