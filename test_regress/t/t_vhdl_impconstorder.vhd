-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- t_vhdl_impconst with the two declarations the other way round: a record
-- constant given what an impure function returns, then an array of them.
-- Whatever elaboration works out about the record's widths, the object's
-- methods run once per call, in declaration order, so the record holds the
-- first two references and the array the next four.

package t_vhdl_impconstorder_pkg is
   type ptr_t is record
      ref : natural;
   end record;
   type queue_t is record
      meta : ptr_t;
      data : ptr_t;
   end record;
   type queue_vec_t is array (natural range <>) of queue_t;
   type counter_t is protected
      impure function next_ref return natural;
   end protected;
   impure function new_queue return queue_t;
end package;

package body t_vhdl_impconstorder_pkg is
   type counter_t is protected body
      variable n : natural := 0;
      impure function next_ref return natural is
      begin
         n := n + 1;
         return n;
      end function;
   end protected body;

   shared variable counter : counter_t;

   impure function new_queue return queue_t is
      variable q : queue_t;
   begin
      q.meta.ref := counter.next_ref;
      q.data.ref := counter.next_ref;
      return q;
   end function;
end package body;

use work.t_vhdl_impconstorder_pkg.all;

entity t_vhdl_impconstorder is
end entity;

architecture a of t_vhdl_impconstorder is
   constant single : queue_t := new_queue;
   constant request_queue : queue_vec_t(1 to 2) := (new_queue, new_queue);
   signal seen : natural := 0;

   impure function ref_of (idx : positive range request_queue'range) return natural is
   begin
      return request_queue(idx).meta.ref;
   end function;
begin
   gen : for i in request_queue'range generate
      g : process
      begin
         assert request_queue(i).meta.ref = 2 * i + 1 report "generate" severity failure;
         wait;
      end process;
   end generate;

   p : process
   begin
      -- Each allocation once, in the order the declarations are elaborated
      assert request_queue(1).meta.ref = 3 report "q1 meta" severity failure;
      assert request_queue(1).data.ref = 4 report "q1 data" severity failure;
      assert request_queue(2).meta.ref = 5 report "q2 meta" severity failure;
      assert request_queue(2).data.ref = 6 report "q2 data" severity failure;
      assert single.meta.ref = 1 report "single" severity failure;
      assert ref_of(2) = 5 report "ref_of" severity failure;
      report "*-* All Finished *-*";
      wait;
   end process;
end architecture;
