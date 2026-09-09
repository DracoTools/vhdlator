// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// === is an exact four-state comparison, so a zero bit is not identical to a
// high-impedance one. A z in the constant can match only an operand that can
// itself be z; an ordinary variable is always driven and never matches.

module t;

  logic [7:0] v = 8'h00;
  logic [7:0] o = 8'hff;
  logic [7:0] m = 8'h5a;

  wire [7:0] bus;
  logic drive = 1'b0;
  assign bus = drive ? 8'ha5 : 8'bz;

  initial begin
    // A variable is always driven, so no bit of it is z
    if ((v[3] === 1'bz) !== 1'b0) $stop;
    if ((o[3] === 1'bz) !== 1'b0) $stop;
    if ((v === 8'bzzzzzzzz) !== 1'b0) $stop;
    if ((m === 8'b0101101z) !== 1'b0) $stop;

    // The constant on either side, and the same answer
    if ((1'bz === v[3]) !== 1'b0) $stop;
    if ((8'bzzzzzzzz === v) !== 1'b0) $stop;

    // !== is the negation of the same question
    if ((v[3] !== 1'bz) !== 1'b1) $stop;
    if ((v !== 8'bzzzzzzzz) !== 1'b1) $stop;
    if ((1'bz !== v[3]) !== 1'b1) $stop;

    // A constant with no z compares as it always did
    if ((v[3] === 1'b0) !== 1'b1) $stop;
    if ((v[3] === 1'b1) !== 1'b0) $stop;
    if ((m === 8'h5a) !== 1'b1) $stop;
    if ((m === 8'h5b) !== 1'b0) $stop;

    // A net nothing drives really is z, and must go on saying so
    #1;
    if ((bus === 8'bzzzzzzzz) !== 1'b1) $stop;
    drive = 1'b1;
    #1;
    if ((bus === 8'bzzzzzzzz) !== 1'b0) $stop;
    if ((bus === 8'ha5) !== 1'b1) $stop;

    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
