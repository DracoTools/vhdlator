-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Recursive subprograms called with values that fold.  Each call gets a copy
-- for its value, and each copy asks for the next -- including from the branch
-- that would not run, since that is analyzed like any other.  A recursion
-- going down asks at last for a value outside its parameter's subtype, and
-- one going up never does, so both have to end some other way than by
-- running out of values.

entity t_vhdl_recval is
end entity;

architecture test of t_vhdl_recval is
   -- Down to zero, then asks for f(-1) from the branch that does not run
   function down (n : natural) return bit_vector is
   begin
      if n = 0 then
         return "1";
      else
         return down(n - 1) & '0';
      end if;
   end function;

   -- The same with a result that is a number, not an array
   function sum (n : natural) return natural is
   begin
      if n = 0 then
         return 0;
      else
         return n + sum(n - 1);
      end if;
   end function;

   -- Upwards to a limit, and past it from the branch that does not run
   function up (n : natural; lim : natural) return bit_vector is
   begin
      if n >= lim then
         return "1";
      else
         return up(n + 1, lim) & '0';
      end if;
   end function;

   -- Two subprograms, each recursing through the other
   function is_odd (n : natural) return boolean;

   function is_even (n : natural) return boolean is
   begin
      if n = 0 then
         return true;
      else
         return is_odd(n - 1);
      end if;
   end function;

   function is_odd (n : natural) return boolean is
   begin
      if n = 0 then
         return false;
      else
         return is_even(n - 1);
      end if;
   end function;
begin
   check : process is
      variable v4 : bit_vector(0 to 3);
      variable v6 : bit_vector(0 to 5);
      variable v101 : bit_vector(0 to 100);
   begin
      v4 := down(3);
      assert v4 = "1000" report "down(3)" severity failure;

      assert sum(10) = 55 report "sum(10)" severity failure;
      -- Deeper than any copy is made for by value
      assert sum(200) = 20100 report "sum(200)" severity failure;

      v6 := up(95, 100);
      assert v6 = "100000" report "up(95, 100)" severity failure;
      -- Deeper than any copy is made for by value, with an array result
      v101 := up(100, 200);
      assert v101(0) = '1' report "up(100, 200) first" severity failure;
      assert v101(100) = '0' report "up(100, 200) last" severity failure;

      assert is_even(10) report "is_even(10)" severity failure;
      assert is_odd(7) report "is_odd(7)" severity failure;
      assert not is_even(7) report "is_even(7)" severity failure;

      report "*-* All Finished *-*";
      wait;
   end process;
end architecture;
