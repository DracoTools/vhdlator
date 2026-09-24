-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A BFM's local is sized by one of its signal parameters -- "variable v :
-- std_logic_vector(rid'length - 1 downto 0)" over "signal rid : in
-- std_logic_vector" -- and handed to a method of a protected type whose formal is open.  That
-- bound names a formal of the BFM, which a copy of the method cannot see, so
-- the call takes the method as declared, the value carrying its bounds.

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_sigformal_pkg is
   type t_counter is protected
      impure function ones (constant value : in std_logic_vector) return natural;
   end protected;
   procedure sample (signal rid : in std_logic_vector; variable q : inout t_counter;
                     variable n : out natural);
end package;

package body t_vhdl_sigformal_pkg is
   type t_counter is protected body
      variable calls : natural := 0;
      impure function ones (constant value : in std_logic_vector) return natural is
         alias a_value : std_logic_vector(value'length - 1 downto 0) is value;
         variable c : natural := 0;
      begin
         calls := calls + 1;
         for i in a_value'range loop
            if a_value(i) = '1' then
               c := c + 1;
            end if;
         end loop;
         return c + 100 * value'length + 1000 * calls;
      end function;
   end protected body;

   procedure sample (signal rid : in std_logic_vector; variable q : inout t_counter;
                     variable n : out natural) is
      variable v : std_logic_vector(rid'length - 1 downto 0);
   begin
      v := rid;
      n := q.ones(v);
   end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_sigformal_pkg.all;

entity t_vhdl_sigformal is
   generic (GW : natural := 6);
end entity;

architecture a of t_vhdl_sigformal is
   signal s : std_logic_vector(GW - 1 downto 0) := "101101";
   signal t : std_logic_vector(2 downto 0) := "001";
begin
   p : process
      variable q : t_counter;
      variable n : natural;
   begin
      sample(s, q, n);
      assert n = 1604 report "six bits gave " & integer'image(n) severity failure;
      sample(t, q, n);
      assert n = 2301 report "three bits gave " & integer'image(n) severity failure;
      report "*-* All Finished *-*";
      wait;
   end process;
end architecture;
