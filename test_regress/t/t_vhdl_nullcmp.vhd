-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- numeric_std answers a comparison with a null operand without comparing
-- anything: "/=" is TRUE and "=", "<", "<=", ">", ">=" are FALSE, each with
-- a warning.  Two empty vectors are not equal by numeric_std, however equal
-- they are as arrays.  Checked where the operands are null in the text and
-- where a slice is null only while running.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_nullcmp is
end entity;

architecture a of t_vhdl_nullcmp is
   function eq (constant l, r : unsigned) return boolean is
   begin
      return l = r;
   end function;
   function ne (constant l, r : unsigned) return boolean is
   begin
      return l /= r;
   end function;
   function lt (constant l, r : unsigned) return boolean is
   begin
      return l < r;
   end function;
   function ge (constant l, r : unsigned) return boolean is
   begin
      return l >= r;
   end function;
begin
   p : process
      variable v : unsigned(7 downto 0) := x"00";
      variable w : unsigned(7 downto 0) := x"00";
      variable k : natural := 0;
   begin
      assert not eq(unsigned'(""), unsigned'("")) report "null =" severity failure;
      assert ne(unsigned'(""), unsigned'("")) report "null /=" severity failure;
      assert not lt(unsigned'(""), unsigned'("")) report "null <" severity failure;
      assert not ge(unsigned'(""), unsigned'("")) report "null >=" severity failure;
      assert not eq(unsigned'(""), unsigned'("0")) report "null = one" severity failure;
      -- Null only while running
      assert not (v(k - 1 downto 0) = w(k - 1 downto 0)) report "k0 =" severity failure;
      assert v(k - 1 downto 0) /= w(k - 1 downto 0) report "k0 /=" severity failure;
      assert not (v(k - 1 downto 0) < w(k - 1 downto 0)) report "k0 <" severity failure;
      assert not (v(k - 1 downto 0) > w(k - 1 downto 0)) report "k0 >" severity failure;
      assert not (v(k - 1 downto 0) <= w(k - 1 downto 0)) report "k0 <=" severity failure;
      assert not (v(k - 1 downto 0) >= w(k - 1 downto 0)) report "k0 >=" severity failure;
      -- And not null: an ordinary comparison
      k := 3;
      assert v(k - 1 downto 0) = w(k - 1 downto 0) report "k3 =" severity failure;
      assert not (v(k - 1 downto 0) /= w(k - 1 downto 0)) report "k3 /=" severity failure;
      assert v(k - 1 downto 0) >= w(k - 1 downto 0) report "k3 >=" severity failure;
      report "*-* All Finished *-*";
      wait;
   end process;
end architecture;
