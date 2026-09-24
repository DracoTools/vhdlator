-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A slice whose length only running settles, of an array whose element's
-- length is written in terms of a generic, handed to a formal that leaves
-- both open.  The element has a length -- one the back end folds for each
-- instance -- so the call gets a copy that carries the count and fixes the
-- element, as it does where the element's length is a number.

package t_vhdl_elemgen_pkg is
   type t_arr is array (natural range <>) of bit_vector;
   procedure count_ones (constant d : in t_arr; variable n : out natural);
end package;

package body t_vhdl_elemgen_pkg is
   procedure count_ones (constant d : in t_arr; variable n : out natural) is
      variable c : natural := 0;
   begin
      for i in d'range loop
         for j in d(i)'range loop
            if d(i)(j) = '1' then
               c := c + 1;
            end if;
         end loop;
      end loop;
      n := c;
   end procedure;
end package body;

use work.t_vhdl_elemgen_pkg.all;

entity t_vhdl_elemgen is
   generic (GN : natural := 4;
            GW : natural := 8);
end entity;

architecture a of t_vhdl_elemgen is
begin
   p : process
      variable v : t_arr(GN - 1 downto 0)(GW - 1 downto 0) := (others => (others => '0'));
      variable k : natural := 2;
      variable n : natural;
   begin
      v(0) := x"0f";
      v(1) := x"01";
      v(2) := x"ff";
      count_ones(v(k - 1 downto 0), n);
      assert n = 5 report "two words gave " & integer'image(n) severity failure;
      k := 3;
      count_ones(v(k - 1 downto 0), n);
      assert n = 13 report "three words gave " & integer'image(n) severity failure;
      report "*-* All Finished *-*";
      wait;
   end process;
end architecture;
