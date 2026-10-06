// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A default value on an input port applies where the port is left
// unconnected (IEEE 1800-2023 23.2.2.4). The top module's inputs are
// connected to nothing in the design, and nothing here drives them, so they
// read their defaults -- although the model wraps the top module in a
// module of its own whose I/O they are connected to.

`define stop $stop
`define checkd(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got=%0d exp=%0d\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0);

module t (
    input logic [3:0] a = 4'hf,
    input int b = 7
);
  initial begin
    #1;
    `checkd(a, 4'hf);
    `checkd(b, 7);
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
