// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// verilog_format: off
`define stop $stop
`define STRINGIFY(x) `"x`"
`define checkd(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got=%0d exp=%0d\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0);
// verilog_format: on

module t;
  integer fd;
  integer r;
  logic [7:0] b;
  logic [8*8-1:0] s;

  initial begin
    fd = $fopen({`STRINGIFY(`TEST_OBJ_DIR), "/closed.dat"}, "w");
    $fwrite(fd, "AB\n");
    $fclose(fd);
    fd = $fopen({`STRINGIFY(`TEST_OBJ_DIR), "/closed.dat"}, "r");
    r = $fgetc(fd);
    `checkd(r, "A");
    $fclose(fd);
    // A descriptor that was closed names no file: reads give EOF, not a crash
    r = $fgetc(fd);
    `checkd(r, -1);
    r = $ungetc("A", fd);
    `checkd(r, -1);
    r = $feof(fd);
    `checkd(r != 0, 1'b1);
    b = 8'h5a;
    r = $fread(b, fd);
    `checkd(r, 0);
    `checkd(b, 8'h5a);
    r = $fgets(s, fd);
    `checkd(r, 0);
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
