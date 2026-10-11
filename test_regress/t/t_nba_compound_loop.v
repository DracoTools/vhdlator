// DESCRIPTION: Verilator: Verilog Test module
//
// Nonblocking assignments in a loop to an array of unpacked structures, in
// a process that does not suspend
//
// This file ONLY is placed under the Creative Commons Public Domain
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

`define stop $stop
`define checkd(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got=%0d exp=%0d\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0);

module t;
  typedef struct {
    int a;
    logic [7:0] b[0:1];
  } rec_t;
  rec_t arr[0:3];
  logic clk = 0;
  int cyc = 0;
  int n = 4;

  initial forever #1 clk = ~clk;

  always @(posedge clk) begin
    cyc <= cyc + 1;
    // n is a variable, so the loop stays a loop
    for (int i = 0; i < n; i++) begin
      arr[i].a <= cyc + i;
      arr[i].b[1] <= 8'(cyc * i);
    end
    if (cyc == 3) begin
      `checkd(arr[0].a, 2);
      `checkd(arr[3].a, 5);
      `checkd(arr[2].b[1], 4);
      `checkd(arr[3].b[1], 6);
      $write("*-* All Finished *-*\n");
      $finish;
    end
  end
endmodule
