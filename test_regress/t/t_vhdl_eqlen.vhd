-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An array type's own "=" is false between operands of two lengths,
-- whatever their elements hold (9.2.3).  Leaving the two widths to differ
-- is not enough: the back end pads the shorter operand, and two arrays of
-- different lengths then compare equal.  Only numeric_std's own
-- comparisons are by value across lengths.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_eqlen is
end entity;

architecture a of t_vhdl_eqlen is
  -- Read through a signal so nothing is settled before it runs
  signal src : std_logic_vector(7 downto 0) := "00111101";

  -- The length comes from the copy made for each call site
  function is_3d(x : std_logic_vector) return boolean is
  begin
    return x = 6x"3d";
  end function;

begin
  process
    variable a8 : std_logic_vector(7 downto 0);
    variable b6 : std_logic_vector(5 downto 0) := "111101";
    variable c8 : std_logic_vector(7 downto 0) := "00111101";
    variable t3 : bit_vector(2 downto 0) := "101";
    variable t4 : bit_vector(3 downto 0) := "0101";
    variable u8 : unsigned(7 downto 0) := "00111101";
    variable u6 : unsigned(5 downto 0) := "111101";
  begin
    wait for 1 ns;
    a8 := src;

    -- Two lengths never compare equal, however the elements read
    assert not (a8 = b6) report "two lengths compared equal" severity failure;
    assert a8 /= b6 report "two lengths did not compare unequal" severity failure;
    assert not (t3 = t4) report "two bit_vector lengths compared equal" severity failure;

    -- One length still compares by its elements
    assert a8 = c8 report "one length did not compare equal" severity failure;
    assert not (a8 = "00111100") report "one length compared wrongly" severity failure;

    -- A sized bit string literal is as long as it says, not as long as
    -- what it is written against
    assert not (a8 = 6x"3d") report "a shorter literal compared equal" severity failure;
    assert a8 = 8x"3d" report "a literal of the same length" severity failure;
    assert a8 = x"3d" report "an unsized hex literal" severity failure;

    -- and through a copy made for the call site
    assert is_3d(6x"3d") report "the same length through a call" severity failure;
    assert not is_3d(x"3d") report "two lengths through a call" severity failure;

    -- numeric_std declares its own, which compare by value across lengths
    assert u8 = u6 report "unsigned compares by value" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
