// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

package pkg;
  bit external_sig;
endpackage

interface iface;
  import pkg::*;

  bit local_sig;

  task local_write;
    // verilator no_inline_task
    local_sig = 1'b1;
  endtask

  task external_write;
    // verilator no_inline_task
    external_sig = 1'b1;
  endtask
endinterface

module t;
  iface i ();
  virtual iface vi;

  initial begin
    // A call through a virtual interface handle is dispatched at run time, so
    // scheduling cannot read the body of the task it reaches
    vi = i;
    vi.local_write();
    vi.external_write();
  end
endmodule
