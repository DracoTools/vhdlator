// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A high-impedance value written by a task, to a variable that is also the
// actual of an inout port the instance drives: the task's statements take
// part in the resolution exactly as the same statements in the process
// calling the task do. V3Tristate did not look inside tasks, so the task's
// drive never met the instance's, and the valid it drove was lost.

module t_tri_task_drive_sink (
    inout logic [1:0] ifc,
    output logic seen
);
  // ifc[1] is the valid the other side drives, ifc[0] the ready this one does
  initial begin
    ifc <= 2'bz1;
    #12 seen = ifc[1];
  end
endmodule

module t;
  logic [1:0] s;
  logic seen;
  t_tri_task_drive_sink u (
      .ifc(s),
      .seen(seen)
  );

  task automatic drive();
    s <= 2'b0z;
    #5;
    s[1] <= 1'b1;
    #1;
  endtask

  initial begin
    drive();
    if (s !== 2'b11) begin
      $display("%%Error: s=%b", s);
      $stop;
    end
    #20;
    if (seen !== 1'b1) begin
      $display("%%Error: seen=%b", seen);
      $stop;
    end
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
