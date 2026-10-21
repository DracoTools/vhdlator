// DESCRIPTION: Verilator: Verilog Test module
//
// A constant function reading back what it wrote into an element of an
// unpacked array held in an unpacked structure, or into a member of a
// structure held in an unpacked array, after the variable was initialized
//
// This file ONLY is placed under the Creative Commons Public Domain
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

`define stop $stop
`define checkd(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got=%0d exp=%0d\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0);

module t;
  typedef struct {
    int steps[4];
    int align[4];
  } props_t;
  typedef struct {
    int a;
    int b;
  } pair_t;
  typedef pair_t pairs_t[3];

  function automatic props_t compute();
    props_t res = '{steps: '{default: 1}, align: '{default: 1}};
    int m;
    for (int i = 0; i < 4; i++) res.steps[i] = 12 - i;
    m = 100;
    for (int i = 0; i < 4; i++) if (res.steps[i] < m) m = res.steps[i];
    for (int i = 0; i < 4; i++) res.align[i] = (i + 1) * (2 ** (res.steps[i] - m));
    return res;
  endfunction

  function automatic pairs_t pairs();
    pairs_t p = '{default: '{a: 0, b: 0}};
    for (int i = 0; i < 3; i++) p[i].a = i + 5;
    for (int i = 0; i < 3; i++) p[i].b = p[i].a * 2;
    return p;
  endfunction

  localparam props_t P = compute();
  localparam pairs_t Q = pairs();
  localparam int ALIGN0 = P.align[0];
  logic [ALIGN0-1:0] wide;

  initial begin
    `checkd(P.steps[0], 12);
    `checkd(P.steps[3], 9);
    `checkd(P.align[0], 8);
    `checkd(P.align[1], 8);
    `checkd(P.align[2], 6);
    `checkd(P.align[3], 4);
    `checkd($bits(wide), 8);
    `checkd(Q[0].b, 10);
    `checkd(Q[2].b, 14);
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
