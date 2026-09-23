-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A bit string literal is a literal of an array of a character type just as
-- a string literal is (15.8), so where a subprogram takes an unconstrained
-- array the literal is handed over carrying its own bounds, exactly as any
-- other value of an unconstrained type is.
--
-- How many elements it has is not how many characters it is written with: a
-- base of 8 or 16 writes three or four elements per character, and a length
-- given in front says its own number outright. Nor is it what the context
-- would like it to be -- that is the rule for a string literal (9.3.2), not
-- for this one.

package bitstrargpkg is
  -- Each gives back something about one operand. None of the three is a
  -- predefined operator for BIT_VECTOR, so these are the only ones.
  function "+" (l, r : bit_vector) return natural;
  function "-" (l, r : bit_vector) return natural;
  function "*" (l, r : bit_vector) return bit;
end package;

package body bitstrargpkg is
  function "+" (l, r : bit_vector) return natural is
  begin
    return r'length;
  end function;
  function "-" (l, r : bit_vector) return natural is
  begin
    return l'length;
  end function;
  function "*" (l, r : bit_vector) return bit is
  begin
    return r(r'left);
  end function;
end package body;

use work.bitstrargpkg.all;

entity t_vhdl_bitstrarg is
end entity;

architecture rtl of t_vhdl_bitstrarg is
  signal s : bit_vector (3 downto 0) := B"1100";
begin
  check : process
  begin
    -- One element per character of a binary literal
    assert (s + B"101") = 3 report "bad length of a binary literal operand" severity failure;
    -- Three per character of an octal one
    assert (s + O"77") = 6 report "bad length of an octal literal operand" severity failure;
    -- Four per character of a hexadecimal one
    assert (s + X"F0") = 8 report "bad length of a hex literal operand" severity failure;
    -- A length written in front says how many elements there are
    assert (s + 6X"3") = 6 report "bad length of a sized literal operand" severity failure;
    -- The same on the left of the operator, not only on the right
    assert (X"AB" - s) = 8 report "bad length of a left-hand literal operand" severity failure;
    -- The operand that is not a literal keeps its own length beside one
    assert (s - X"F0") = 4 report "bad length beside a literal operand" severity failure;
    assert (B"101" + s) = 4 report "bad length beside a left-hand literal" severity failure;
    -- And the elements arrive the way round they were written
    assert (s * B"101") = '1' report "bad leftmost element of a binary literal" severity failure;
    assert (s * B"011") = '0' report "bad leftmost element of a binary literal" severity failure;
    assert (s * X"F0") = '1' report "bad leftmost element of a hex literal" severity failure;
    assert (s * X"0F") = '0' report "bad leftmost element of a hex literal" severity failure;
    report "*-* All Finished *-*";
    std.env.stop;
    wait;
  end process;
end architecture;
