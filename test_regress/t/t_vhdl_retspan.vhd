-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Two return paths whose lengths are written rather than folded can still be
-- shown to agree with each other.  A range and its reverse, "0 to n - 1" and
-- "n - 1 downto 0", are as long as each other whatever n turns out to be, so
-- the result has a length though neither path says it as a number.

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_retspan_pkg is
   type t_slv_array is array (natural range <>) of std_logic_vector;
   function norm(constant value : in t_slv_array; constant target : in t_slv_array)
      return t_slv_array;
end package;

package body t_vhdl_retspan_pkg is
   -- One of two arrays as long as each other is filled and returned, the one
   -- that runs the way the value does
   function norm(constant value : in t_slv_array; constant target : in t_slv_array)
      return t_slv_array is
      variable v_asc : t_slv_array(0 to target'length - 1)(0 to target(target'low)'length - 1);
      variable v_dsc : t_slv_array(target'length - 1 downto 0)(target(target'low)'length - 1 downto 0);
   begin
      if value'ascending then
         for i in 0 to value'length - 1 loop
            v_asc(i) := value(value'low + i);
         end loop;
         return v_asc;
      else
         for i in 0 to value'length - 1 loop
            v_dsc(v_dsc'high - i) := value(value'high - i);
         end loop;
         return v_dsc;
      end if;
   end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_retspan_pkg.all;

entity t_vhdl_retspan is
   -- The bounds name generics, so nothing folds them here
   generic (GC_N : natural := 4;
            GC_W : natural := 8);
end entity;

architecture a of t_vhdl_retspan is
begin
   p : process
      variable dn : t_slv_array(GC_N - 1 downto 0)(GC_W - 1 downto 0);
      variable rd : t_slv_array(GC_N - 1 downto 0)(GC_W - 1 downto 0);
      variable da : t_slv_array(0 to GC_N - 1)(0 to GC_W - 1);
      variable oa : t_slv_array(0 to GC_N - 1)(0 to GC_W - 1);
   begin
      -- The descending path
      dn(3) := x"5a";
      dn(2) := x"3c";
      dn(1) := x"0f";
      dn(0) := x"f0";
      rd := norm(dn, rd);
      assert rd(3) = x"5a" report "descending element 3" severity failure;
      assert rd(0) = x"f0" report "descending element 0" severity failure;
      assert rd'length = GC_N report "descending length" severity failure;

      -- The ascending path, whose returned array runs the other way and is
      -- just as long
      da(0) := x"11";
      da(3) := x"44";
      oa := norm(da, oa);
      assert oa(0) = x"11" report "ascending element 0" severity failure;
      assert oa(3) = x"44" report "ascending element 3" severity failure;
      assert oa'length = GC_N report "ascending length" severity failure;

      report "*-* All Finished *-*";
      wait;
   end process;
end architecture;
