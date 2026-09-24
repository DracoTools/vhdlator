-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An aggregate whose one choice is a null range is a null array of that range
-- (9.3.3.3): "(v'range => '0')" of a null v written "0 to -1" has no elements,
-- not the two from -1 to 0 that its bounds span.  By an array type's own "="
-- two null arrays are equal; by numeric_std's it is FALSE, since numeric_std
-- compares no null operand.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_nullagg is
end entity;

architecture a of t_vhdl_nullagg is
   function all_zero (constant v : bit_vector) return boolean is
   begin
      return v = (v'range => '0');
   end function;
   function agg_len (constant v : bit_vector) return integer is
      constant c : bit_vector := (v'range => '1');
   begin
      return c'length;
   end function;
   function is_zero (constant value : unsigned) return boolean is
   begin
      if value = (value'range => '0') then
         return true;
      end if;
      return false;
   end function;
begin
   p : process
      variable z : unsigned(3 downto 0) := "0000";
      variable o : unsigned(3 downto 0) := "0100";
   begin
      assert all_zero(bit_vector'("")) report "null all_zero" severity failure;
      assert agg_len(bit_vector'("")) = 0 report "null agg_len" severity failure;
      assert all_zero(bit_vector'("0000")) report "four all_zero" severity failure;
      assert agg_len(bit_vector'("0000")) = 4 report "four agg_len" severity failure;
      assert not is_zero(unsigned'("")) report "null" severity failure;
      assert is_zero(z) report "zero" severity failure;
      assert not is_zero(o) report "nonzero" severity failure;
      report "*-* All Finished *-*";
      wait;
   end process;
end architecture;
