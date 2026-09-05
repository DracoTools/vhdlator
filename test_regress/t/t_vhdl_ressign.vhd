-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- RESIZE fills what the result is short with the operand's leftmost
-- element -- its sign -- and writes the low elements over that, so a
-- SIGNED keeps its sign leftmost whichever of the two lengths is the
-- greater (9.3.6).  Widening a signed value is what every accumulator and
-- every sign-extended datapath is written with.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_ressign is
end entity;

architecture a of t_vhdl_ressign is

  -- Read through a call, so what is resized is not a plain object
  impure function bump(v : signed(7 downto 0)) return signed is
  begin
    return v + 1;
  end function;

begin
  process
    variable neg : signed(7 downto 0) := "11110110";    -- -10
    variable pos : signed(7 downto 0) := "00001010";    -- 10
    variable u   : unsigned(7 downto 0) := "11110110";
  begin
    -- Wider: the sign runs up into the new elements
    assert resize(neg, 12) = "111111110110" report "signed did not widen with its sign"
      severity failure;
    assert resize(pos, 12) = "000000001010" report "positive widened wrong"
      severity failure;

    -- Shorter: the sign stays leftmost, whatever it displaces
    assert resize(neg, 4) = "1110" report "truncation dropped the sign" severity failure;
    assert resize(pos, 4) = "0010" report "positive truncated wrong" severity failure;
    assert resize(neg, 1) = "1" report "one element is the sign alone" severity failure;

    -- The same length is the value itself
    assert resize(neg, 8) = "11110110" report "unchanged length differs" severity failure;

    -- An UNSIGNED pads and truncates with no sign at all
    assert resize(u, 12) = "000011110110" report "unsigned widened wrong" severity failure;
    assert resize(u, 4) = "0110" report "unsigned truncated wrong" severity failure;

    -- What is resized is read once, however it is written
    assert resize(bump(neg), 12) = "111111110111" report "resize of a call"
      severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
