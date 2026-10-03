// DESCRIPTION: Verilator: Verilog Test module
//
// An unpacked-array parameter whose range names an earlier parameter: the
// pattern an instance gives it is as long as that instance makes the range.
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

module sub #(
    parameter int N = 1,
    parameter int FOO[0:N-1] = '{default: 0},
    parameter int BAR[N-1:0] = '{default: 0}
) ();
  function automatic int total();
    int sum = 0;
    for (int i = 0; i < N; i++) sum += FOO[i] * (i + 1) + BAR[i];
    return sum;
  endfunction
  localparam int SUM = total();
endmodule

module nodflt #(
    parameter int N,
    parameter int FOO[0:N-1]
) ();
  localparam int LAST = FOO[N-1];
endmodule

module t;
  sub #(2, '{4, 1}, '{5, 3}) u1 ();
  sub #(.N(3), .FOO('{1, 2, 3}), .BAR('{10, 20, 30})) u2 ();
  sub u3 ();
  nodflt #(.N(4), .FOO('{9, 8, 7, 6})) u4 ();
  initial begin
    // FOO[0]*1 + FOO[1]*2 + BAR[0] + BAR[1]
    if (u1.SUM != 4 + 2 + 3 + 5) $stop;
    if (u2.SUM != 1 + 4 + 9 + 60) $stop;
    if (u3.SUM != 0) $stop;
    if (u4.LAST != 6) $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
