// DESCRIPTION: Verilator: Verilog Test module
//
// Nonblocking assignments in a loop of a process that suspends, to parts of
// an unpacked structure and to elements of an array of them
//
// This file ONLY is placed under the Creative Commons Public Domain
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

`define stop $stop
`define checkh(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got='h%x exp='h%x\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0);

module t;
  typedef struct {
    logic trigger;
    logic [7:0] words[0:7];
  } msg_t;
  typedef struct {
    int x;
    logic [7:0] y[1:3];
  } rec_t;

  msg_t msg;
  rec_t recs[1:4];
  int n = 3;

  initial begin
    for (int i = 0; i < 8; i++) msg.words[i] = 8'hee;
    #1;
    // n is a variable, so the loops stay loops
    for (int i = 0; i < n; i++) msg.words[i] <= 8'(i + 6);
    msg.trigger <= 1'b1;
    for (int j = 1; j <= n + 1; j++) begin
      recs[j].x <= j * 10;
      recs[j].y[2] <= 8'(j);
    end
    #1;
    `checkh(msg.words[0], 8'h06);
    `checkh(msg.words[1], 8'h07);
    `checkh(msg.words[2], 8'h08);
    `checkh(msg.words[3], 8'hee);
    `checkh(msg.trigger, 1'b1);
    `checkh(recs[1].x, 10);
    `checkh(recs[4].x, 40);
    `checkh(recs[3].y[2], 8'h03);
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
