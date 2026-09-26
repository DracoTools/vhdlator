// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// Statements in tasks that the tristate pass leaves as they were before it
// looked into tasks: a force, and a case comparison with high impedance of
// a variable a package owns, which reads an enable of another module's.

package t_tri_task_force_pkg;
  logic [1:0] flags = 2'bz0;
endpackage

module t;
  import t_tri_task_force_pkg::*;
  logic line;
  int waited = 0;

  task automatic poke();
    force line = 1'b1;
    #1;
    release line;
  endtask

  task automatic wait_flag();
    while (flags[1] === 1'bz && waited < 3) begin
      #1;
      waited++;
    end
  endtask

  initial begin
    line = 1'b0;
    poke();
    wait_flag();
    flags[1] = 1'b1;
    wait_flag();
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
