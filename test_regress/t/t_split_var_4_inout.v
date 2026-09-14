// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain, for
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

module t(/*AUTOARG*/);

   typedef struct packed {
      logic [31:0]      n;
      logic [15:0][7:0] f;
   } t_rec;

   t_rec r /*verilator split_var*/;
   int   alen;

   // An inout argument both reads and writes the part select it is given,
   // so the reference appears among the reads and among the writes
   task automatic bump(inout logic [127:0] c, output int len);
      c = c + 128'd1;
      len = 4;
   endtask

   initial begin
      r.n = 32'hdead;
      r.f = 128'd41;
      bump(r.f, alen);
      if (r.n !== 32'hdead) $stop;   // The bits beside it are untouched
      if (r.f !== 128'd42) $stop;    // Read, changed and written back
      if (alen !== 4) $stop;
      $write("*-* All Finished *-*\n");
      $finish;
   end

endmodule
