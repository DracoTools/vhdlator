// DESCRIPTION: Verilator: Verilog Test module
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of either the GNU Lesser General Public License Version 3
// or the Perl Artistic License Version 2.0.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

// Logic reading a field of an unpacked struct is not made into a table: each
// row of one sets its inputs to numbers.

`define stop $stop
`define checkh(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got='h%x exp='h%x\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0);

module t;
  typedef struct {
    logic [3:0] h;
    logic [3:0] f;
    logic [3:0] g;
  } s_t;
  s_t s;
  logic [1:0] n;

  assign n = 2'(8'hb4 >> (3'(((5'h11 >= 5'({s.f, 1'b0}))
                                ? 3'(2'(18'h1e5e4 >> 5'({s.f, 1'b0}))) : 3'h0) << 1)));

  initial begin
    s.f = 4'h2;
    s.g = 4'h1;
    s.h = 4'h3;
    #1;
    `checkh(n, 2'h3);
    s.f = 4'h3;
    #1;
    `checkh(n, 2'h2);
    s.f = 4'h9;
    #1;
    `checkh(n, 2'h0);
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
