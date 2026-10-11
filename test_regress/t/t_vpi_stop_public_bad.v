// DESCRIPTION: Verilator: Verilog Test module
//
// vpiStop with a public signal, so that the VPI hierarchy is not empty when
// the model held at file scope is destroyed by the exit $stop ends in
//
// This file ONLY is placed under the Creative Commons Public Domain
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

module t;
  import "DPI-C" function void dpii_test();

  int visible  /*verilator public_flat_rw*/ = 1;

  initial begin
    dpii_test();
    $display("Should have stopped above");
    $finish;
  end
endmodule
