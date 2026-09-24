-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An aggregate of one range choice whose bounds only elaboration or running
-- settle, given to an object whose subtype fixes its length.  The aggregate
-- is as long as its range says (9.3.3.3), and that has to be the object's
-- length (10.5.2.1): where the two agree every element is set, and where they
-- do not it is an error while running -- not a fill of whatever the object's
-- length is.

entity t_vhdl_aggdynrng is
   generic (LO : natural := 0;
            HI : natural := 15);
end entity;

architecture a of t_vhdl_aggdynrng is
   constant c : bit_vector(0 to 15) := (LO to HI => '1');
   -- VESTS tc753 writes its constants this way: a constrained array type,
   -- which is its own base, given a range running the other way
   type cons_t is array (15 downto 0) of boolean;
   constant cb : cons_t := (LO to HI => true);
begin
   p : process
      variable r : bit_vector(1 to 3) := "000";
      variable n : natural := 3;
   begin
      assert c = x"FFFF" report "generic range" severity failure;
      assert cb = (cons_t'range => true) report "constrained type" severity failure;
      r := (1 to n => '1');
      assert r = "111" report "running range" severity failure;
      r := (n downto 1 => '0');
      assert r = "000" report "running downto" severity failure;
      report "*-* All Finished *-*";
      -- And one that does not match: NVC's bounds24, three bits from a
      -- thousand-element aggregate
      n := 1000;
      r := (1 to n => '1');
      wait;
   end process;
end architecture;
