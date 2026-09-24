// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// Each dereference of a handle is null checked, and the check hands the
// handle on by reference: a copy per dereference was a reference counted
// in and out, and 256 of them in one suspendable process cost g++ minutes.
// The handle must still reach whatever uses it, including a handle only the
// expression holds, and one dropped while its task is suspended.

class Node;
  static int finished;
  int v;
  Node next;
  int arr[16][16];
  function new(int v);
    this.v = v;
  endfunction
  function Node self();
    return this;
  endfunction
  task automatic bump_later();
    #5;
    v++;
    finished = v;
  endtask
endclass

module t;
  Node a, b;
  int ref2[16][16];

  function automatic Node make(int v);
    Node n = new(v);
    return n;
  endfunction

  initial begin
    a = new(1);
    a.next = new(2);
    a.next.next = new(3);
    if (a.next.next.v != 3) $stop;
    if (make(9).v != 9) $stop;  // A handle only the expression holds
    if (a.self().next.self().v != 2) $stop;

    for (int i = 0; i < 16; i++) for (int j = 0; j < 16; j++) ref2[i][j] = i * 16 + j;
    #1;
    a.arr = ref2;  // A dereference per element, in a suspendable process
    if (a.arr != ref2) $stop;
    a.arr[15][15] = 0;
    if (a.arr == ref2) $stop;

    b = a.next;
    fork
      #2 a = null;  // The only handle dropped while its task waits
    join_none
    a.bump_later();
    if (a != null) $stop;
    if (Node::finished != 2) $stop;
    if (b.v != 2 || b.next.v != 3) $stop;

    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
