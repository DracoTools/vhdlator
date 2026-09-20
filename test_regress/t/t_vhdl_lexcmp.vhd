-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- The relational operators over an array run element by element, and where
-- every element the two share is equal the shorter operand is the lesser
-- (5.2.6). Thirty-two zeros are therefore greater than sixteen, which no
-- comparison of the two as numbers would ever say. numeric_std's own
-- operators are not these: they compare what the operands are worth.

library ieee;
use ieee.numeric_std.all;
use std.env.all;

entity t_vhdl_lexcmp is
end entity;

architecture tb of t_vhdl_lexcmp is
begin
  main: process is
    variable w32 : bit_vector(1 to 32)  := x"00000000";
    variable w16 : bit_vector(15 downto 0) := x"0000";
    variable f32 : bit_vector(1 to 32)  := x"ffffffff";
    variable f16 : bit_vector(15 downto 0) := x"ffff";
    variable s8  : signed(7 downto 0)   := to_signed(-10, 8);
    variable s4  : signed(3 downto 0)   := to_signed(5, 4);
  begin
    -- The longer operand wins where the shared elements are equal
    assert w32 > w16 report "32 zeros are not greater than 16" severity failure;
    assert w16 < w32 report "16 zeros are not less than 32" severity failure;
    assert f32 > f16 report "32 ones are not greater than 16" severity failure;
    assert not (w32 <= w16) report "32 zeros are at most 16" severity failure;

    -- ... and the first element that differs decides before length does
    assert w32 < f16 report "zeros are not less than ones" severity failure;
    assert f16 > w32 report "ones are not greater than zeros" severity failure;

    -- Equality is false between two lengths whatever they hold
    assert w32 /= w16 report "two lengths compared equal" severity failure;

    -- At one length nothing changes
    assert w16 < f16 report "zeros are not less than ones at one length"
      severity failure;
    assert f16 = f16 report "ones do not equal themselves" severity failure;

    -- numeric_std compares by value, across lengths and all
    assert s8 < s4 report "-10 is not less than 5" severity failure;
    assert not (s4 < s8) report "5 is less than -10" severity failure;
    stop(0);
  end process;
end architecture;
