// DESCRIPTION: Verilator: Verilog Test module
//
// High impedance in the initial value of an unpacked array, which is data
// rather than the driver of a net
//
// This file ONLY is placed under the Creative Commons Public Domain
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

`define stop $stop
`define checkh(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got='h%x exp='h%x\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0);

module t;
  logic [3:0] seq[0:2] = '{4'b1111, 4'bzzzz, 4'b0101};
  localparam logic [3:0] TBL[2] = '{4'bz01z, 4'b1001};

  initial begin
    `checkh(seq[0], 4'b1111);
    `checkh(seq[2], 4'b0101);
    `checkh(TBL[1], 4'b1001);
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
