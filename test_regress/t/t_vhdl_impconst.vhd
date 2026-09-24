-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An architecture's constant array of handles, each allocated by an impure
-- function -- VUnit's "constant request_queue : queue_vec_t(1 to 2) :=
-- (new_queue, new_queue)" -- whose range a subprogram's parameter subtype and
-- a generate are written in terms of.  The range is its subtype's and static;
-- the value is what running the allocator gives, once and in order, so the
-- constant is an object given its value rather than a parameter.

package t_vhdl_impconst_pkg is
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

package body t_vhdl_impconst_pkg is
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

use work.t_vhdl_impconst_pkg.all;

entity t_vhdl_impconst is
end entity;

architecture a of t_vhdl_impconst is
   constant request_queue : queue_vec_t(1 to 2) := (new_queue, new_queue);
   constant single : queue_t := new_queue;
   signal seen : natural := 0;

   impure function ref_of (idx : positive range request_queue'range) return natural is
   begin
      return request_queue(idx).meta.ref;
   end function;
begin
   gen : for i in request_queue'range generate
      g : process
      begin
         assert request_queue(i).meta.ref = 2 * i - 1 report "generate" severity failure;
         wait;
      end process;
   end generate;

   p : process
   begin
      -- Each allocation once, in the order the declarations are elaborated
      assert request_queue(1).meta.ref = 1 report "q1 meta" severity failure;
      assert request_queue(1).data.ref = 2 report "q1 data" severity failure;
      assert request_queue(2).meta.ref = 3 report "q2 meta" severity failure;
      assert request_queue(2).data.ref = 4 report "q2 data" severity failure;
      assert single.meta.ref = 5 report "single" severity failure;
      assert ref_of(2) = 3 report "ref_of" severity failure;
      report "*-* All Finished *-*";
      wait;
   end process;
end architecture;
