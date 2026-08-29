-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_operspec is
end entity;

architecture a of t_vhdl_operspec is

  -- An operator a design declares is a subprogram whose name happens to be
  -- an operator symbol (4.5.2), so it is copied per use in the same way.
  -- This one is std_logic_unsigned's: the result is as long as the two
  -- operands together, which only the use says.
  function "*" (l, r : std_logic_vector) return std_logic_vector is
    variable res : std_logic_vector(l'length + r'length - 1 downto 0);
  begin
    res := std_logic_vector(to_unsigned(to_integer(unsigned(l)) * to_integer(unsigned(r)),
                                        l'length + r'length));
    return res;
  end function;

  function "-" (v : std_logic_vector) return std_logic_vector is
    variable res : std_logic_vector(v'length downto 0);
  begin
    res := std_logic_vector(to_unsigned(2 ** v'length - to_integer(unsigned(v)), v'length + 1));
    return res;
  end function;

begin

  main : process is
    variable a8 : std_logic_vector(7 downto 0) := x"05";
    variable b4 : std_logic_vector(3 downto 0) := x"3";
    variable p12 : std_logic_vector(11 downto 0);
    variable p8 : std_logic_vector(7 downto 0);
    variable n5 : std_logic_vector(4 downto 0);
  begin
    p12 := a8 * b4;
    assert p12'length = 12 report "product length" severity failure;
    assert to_integer(unsigned(p12)) = 15 report "product value" severity failure;

    -- The same operator used again at another pair of lengths
    p8 := b4 * b4;
    assert p8'length = 8 report "second product length" severity failure;
    assert to_integer(unsigned(p8)) = 9 report "second product value" severity failure;

    n5 := -b4;
    assert n5'length = 5 report "negation length" severity failure;
    assert to_integer(unsigned(n5)) = 13 report "negation value" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;

end architecture;
