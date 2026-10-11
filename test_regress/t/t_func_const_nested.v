// DESCRIPTION: Verilator: Verilog Test module
//
// Constant functions writing through more than one level of unpacked
// structures and arrays, and elements that are packed structures
//
// This file ONLY is placed under the Creative Commons Public Domain
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

`define stop $stop
`define checkd(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got=%0d exp=%0d\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0);

module t;
  typedef struct packed {
    logic [15:0] re;
    logic [15:0] im;
  } cplx_t;
  typedef cplx_t cvec_t[0:3];

  typedef struct packed {
    logic [7:0] lo;
    logic [7:0] hi;
  } info_t;
  typedef struct {
    int first;
    info_t list[0:2];
    int last;
  } range_t;

  function automatic cvec_t weights();
    cvec_t w;
    for (int k = 0; k < 4; k++) w[k] = '{re: 16'(k * 3), im: 16'(100 - k)};
    return w;
  endfunction

  function automatic range_t ranges();
    range_t v;
    int pos = 0;
    v.first = 7;
    for (int i = 0; i < 3; i++) begin
      v.list[i].lo = 8'(pos);
      pos = pos + 4;
      v.list[i].hi = 8'(pos - 1);
    end
    v.list[1].hi[7:4] = 4'hf;
    v.last = pos;
    return v;
  endfunction

  localparam cvec_t W = weights();
  localparam range_t R = ranges();

  initial begin
    `checkd(W[0].re, 0);
    `checkd(W[3].re, 9);
    `checkd(W[2].im, 98);
    `checkd(R.first, 7);
    `checkd(R.list[0].lo, 0);
    `checkd(R.list[2].hi, 11);
    `checkd(R.list[1].hi, 8'hf7);
    `checkd(R.last, 12);
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
