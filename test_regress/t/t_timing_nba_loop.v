// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// Nonblocking assignments to bits of a packed variable, in a loop, in a
// process that suspends. Each such assignment held one value in a flag of
// its own, so the loop committed only its last write.

// verilog_format: off
`define stop $stop
`define checkh(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got='h%x exp='h%x\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0)
// verilog_format: on

module t;
  logic [7:0] dec = 8'hff;
  logic [99:0] wide = '0;
  logic [7:0] forked = '0;

  initial begin
    for (int i = 0; i < 4; i++) begin
      dec <= 8'hff;  // Whole, then parts over it
      for (int k = 0; k <= i; k++) dec[k] <= 1'b0;
      #1;
      `checkh(dec, 8'hff << (i + 1));
    end
    for (int k = 0; k < 100; k += 3) wide[k] <= 1'b1;
    #1;
    `checkh(wide[0], 1'b1);
    `checkh(wide[99], 1'b1);
    `checkh(wide[98], 1'b0);
    `checkh(wide[51], 1'b1);
    fork
      begin
        for (int k = 0; k < 8; k += 2) forked[k] <= 1'b1;
      end
    join
    #1;
    `checkh(forked, 8'h55);
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
