// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A task writing high impedance into a variable a package owns. The
// tristate pass does not resolve across modules, so such a write stays an
// ordinary write to the variable, as it was before tasks were looked into.

package t_tri_task_pkgvar_pkg;
  logic [3:0] runner = 4'bzz10;
endpackage

module t;
  import t_tri_task_pkgvar_pkg::*;

  task automatic set_idx(input int idx, input logic v);
    runner[idx] = v;
  endtask

  initial begin
    set_idx(2, 1'b1);
    if (runner[2] !== 1'b1) $stop;
    set_idx(2, 1'bz);
    set_idx(0, 1'b1);
    if (runner[1:0] !== 2'b11) $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
