// DESCRIPTION: Verilator: Verilog Test module
//
// A constant function writing a member of a packed structure that is an
// element of an unpacked array
//
// This file ONLY is placed under the Creative Commons Public Domain
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

`define stop $stop
`define checkh(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got='h%x exp='h%x\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0);

module t;
  typedef struct packed {
    logic [31:0] base;
    logic [7:0]  bits;
  } cfg_t;
  typedef cfg_t cfg_arr_t[0:2];
  typedef logic [1:0][3:0] nib_t;
  typedef nib_t nib_arr_t[0:1];

  function automatic cfg_arr_t gen(input int n);
    cfg_arr_t r;
    for (int i = 0; i < 3; i++) begin
      r[i].base = 32'h1000 * i;
      r[i].bits = 8'(n + i);
    end
    // A part of a member already written
    r[2].base[3:0] = 4'h5;
    return r;
  endfunction

  function automatic nib_arr_t nibs();
    nib_arr_t r;
    r[0] = '0;
    r[1] = '0;
    r[0][1] = 4'ha;
    r[1][0] = 4'h3;
    return r;
  endfunction

  localparam cfg_arr_t C = gen(5);
  localparam nib_arr_t N = nibs();
  logic [C[1].bits-1:0] w;

  initial begin
    `checkh(C[0].base, 32'h0);
    `checkh(C[1].base, 32'h1000);
    `checkh(C[2].base, 32'h2005);
    `checkh(C[0].bits, 8'd5);
    `checkh(C[2].bits, 8'd7);
    `checkh($bits(w), 6);
    `checkh(N[0], 8'ha0);
    `checkh(N[1], 8'h03);
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
