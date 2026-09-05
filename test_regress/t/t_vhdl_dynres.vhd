-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- RESIZE lines the two right-hand ends up: what falls off the left is
-- lost, and what the result is short is filled with zeros for an UNSIGNED
-- and with the sign for a SIGNED, whose leftmost element stays the sign
-- however the two lengths compare (9.3.6).  Where the length is only
-- decided by running, the result is built from the operand's own elements
-- -- an operand wider than a machine word, or a negative one, has no
-- number that stands for it.  OSVVM's RandSigned hands RESIZE a value
-- that already carries its own bounds.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_dynres is
end entity;

architecture a of t_vhdl_dynres is

  -- As long as running says, so the result carries its own bounds
  function grow(v : signed; n : natural) return signed is
  begin
    return resize(v, v'length + n);
  end function;

  function ugrow(v : unsigned; n : natural) return unsigned is
  begin
    return resize(v, v'length + n);
  end function;

  -- Cut short, again to a length only running decides
  function shrink(v : signed; n : natural) return signed is
  begin
    return resize(v, v'length - n);
  end function;

  -- The shape OSVVM's RandSigned has: what RESIZE is handed already
  -- carries its own bounds
  function span(mn, mx : signed) return signed is
    constant len : integer := maximum(mx'length, mn'length);
  begin
    return resize(resize(mx, len + 1) - resize(mn, len + 1), len);
  end function;

begin
  process
    variable n    : natural := 4;
    variable neg  : signed(7 downto 0) := "11110110";   -- -10
    variable big  : unsigned(39 downto 0) := x"FF00000001";
    variable mn   : signed(7 downto 0) := "11111011";   -- -5
    variable mx   : signed(7 downto 0) := "00001010";   -- 10
    variable wide : unsigned(43 downto 0);
  begin
    -- A negative value widens with its sign, not with zeros
    assert grow(neg, n) = "111111110110"
      report "signed resize did not widen with its sign" severity failure;
    assert grow(neg, n)'length = 12 report "wrong length" severity failure;

    -- Nothing above a machine word is lost
    wide := ugrow(big, n);
    assert wide = x"0FF00000001"
      report "unsigned resize lost the top of a wide value" severity failure;
    assert wide(39 downto 32) = x"FF" report "top byte lost" severity failure;
    assert wide(43 downto 40) = "0000" report "not zero filled" severity failure;

    -- Cut short, a SIGNED keeps its sign leftmost
    assert grow(neg, 0) = "11110110" report "unchanged length differs" severity failure;
    assert shrink(neg, n) = "1110" report "truncation dropped the sign" severity failure;

    -- What RESIZE is handed already carries its own bounds
    assert span(mn, mx) = "00001111" report "wrong span" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
