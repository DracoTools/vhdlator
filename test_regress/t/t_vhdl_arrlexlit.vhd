-- DESCRIPTION: Verilator: VHDL array ordering against a literal of another length
--
-- This file is part of Verilator.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
--
-- The relational operators over a one-dimensional array are lexicographic:
-- the first pair of elements that differ decides, and where one operand runs
-- out first the shorter is the lesser (9.2.3). A string or bit string
-- literal has no constrained subtype of its own, and against one of another
-- length the two were compared as numbers, so "00" < "000" was false. NVC's
-- operator3; nvc 1.22.1 passes this test.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_arrlexlit is
end entity;

architecture test of t_vhdl_arrlexlit is
begin
  process is
    variable y : bit_vector(1 to 3);
    variable d : bit_vector(2 downto 0);
    variable s : std_logic_vector(3 downto 0);
  begin
    y := "000";
    assert "00" < y report "short literal on the left" severity failure;
    assert not (y <= "00") report "short literal on the right" severity failure;
    assert not ("00" > y) report "short literal, >" severity failure;
    assert y > "00" and y >= "00" report "longer object" severity failure;
    assert y < "0000" and not (y >= "0000") report "longer literal" severity failure;
    assert not (y < "000") and y <= "000" and y >= "000" report "same length" severity failure;
    y := "011";
    assert y < "1" and y > "01" and y < "0110" report "first difference decides" severity failure;
    d := "100";
    assert d > "0111" and d < "11" and d > "1" report "descending object" severity failure;
    s := "0101";
    assert s > "01" and s < "011" and "1" > s report "std_logic_vector" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
