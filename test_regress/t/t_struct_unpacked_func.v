// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A function called where a constant is required is evaluated at elaboration,
// and what it returns may be a structure that does not pack -- a design that
// works out its own configuration and then sizes something from it.

module t;

   typedef struct {
      int  width;
      real prob;
   } cfg_t;

   typedef struct {
      int lo;
      int hi;
   } pair_t;

   // Built a member at a time
   function automatic cfg_t decode(int n);
      cfg_t r;
      r.width = n;
      r.prob = 1.0;
      return r;
   endfunction

   // Built whole, and read back a member of it
   function automatic pair_t span(int n);
      pair_t p;
      p = '{lo: n, hi: n + 3};
      return p;
   endfunction

   function automatic int widen(cfg_t c);
      return c.width + 2;
   endfunction

   localparam cfg_t K = decode(6);
   localparam pair_t P = span(4);
   localparam int W = widen(decode(5));

   logic [K.width-1:0] v;
   logic [P.hi-P.lo:0] w;

   initial begin
      v = '1;
      w = '1;
      if ($bits(v) != 6) $stop;
      if ($bits(w) != 4) $stop;
      if (K.width != 6) $stop;
      if (K.prob != 1.0) $stop;
      if (P.lo != 4) $stop;
      if (P.hi != 7) $stop;
      if (W != 7) $stop;
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
