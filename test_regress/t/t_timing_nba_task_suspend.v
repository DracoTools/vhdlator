// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// A nonblocking assignment made after a process resumes inside a task it
// called: the update must be performed however the process was resumed
// (IEEE 1800-2023 10.4.2). The commit was sensitive only to the events the
// process itself waited on, not to those of a task that is not inlined --
// a class method -- so where nothing else happened, the update was lost.

// verilog_format: off
`define stop $stop
`define checkd(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got=%0d exp=%0d\n", `__FILE__,`__LINE__, (gotv), (expv)); `stop; end while(0)
// verilog_format: on

package p;
  logic go = 0;
  event ev;
  class C;
    static task wait_go();
      @(posedge go);
    endtask
    // Suspends only through the task it calls
    static task wait_go_twice_removed();
      wait_go();
    endtask
    static task wait_ev();
      @ev;
    endtask
  endclass
endpackage

module t;
  import p::*;
  logic start = 0;
  logic done_a = 0, done_b = 0, done_c = 0;

  initial begin
    @(posedge start);
    C::wait_go();
    done_a <= 1;
  end

  initial begin
    @(posedge start);
    C::wait_go_twice_removed();
    done_b <= 1;
  end

  initial begin
    @(posedge start);
    C::wait_ev();
    done_c <= 1;
  end

  initial begin
    start = 1;
    #1 go = 1;
    #1 ->ev;
    #1;
    `checkd(done_a, 1'b1);
    `checkd(done_b, 1'b1);
    `checkd(done_c, 1'b1);
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
