// DESCRIPTION: Verilator: Verilog Test module
//
// A constant function writing part of an element of an unpacked array
//
// This file ONLY is placed under the Creative Commons Public Domain
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

`define stop $stop
`define checkh(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got='h%x exp='h%x\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0);

module t;
  typedef logic [7:0] tbl_t[0:3];

  function automatic tbl_t mk();
    tbl_t r;
    for (int i = 0; i < 4; i++) begin
      // Bit by bit, then a part select over what is already there
      for (int j = 0; j < 8; j++) r[i][j] = 1'((i * 5 + 3) >> j);
      if (i == 2) r[i][7:4] = 4'ha;
    end
    return r;
  endfunction

  localparam tbl_t P = mk();

  initial begin
    `checkh(P[0], 8'h03);
    `checkh(P[1], 8'h08);
    `checkh(P[2], 8'had);
    `checkh(P[3], 8'h12);
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
