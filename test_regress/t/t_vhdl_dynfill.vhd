-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- "(v'range => '0')" is as long as the value it is written against, and
-- where that length is only decided by running, so is the aggregate
-- (9.3.3.3).  VUnit and OSVVM both ask "is this all zeros?" that way.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_dynfill is
end entity;

architecture a of t_vhdl_dynfill is

  -- The result is as long as the argument says, so the length of "grown"
  -- below is not decided until it runs
  function grow(v : unsigned; n : natural) return unsigned is
  begin
    return resize(v, v'length + n);
  end function;

  function is_zero(v : unsigned) return boolean is
  begin
    return v = (v'range => '0');
  end function;

begin
  process
    variable n : natural := 3;
    variable zeros : unsigned(7 downto 0) := (others => '0');
    variable ones : unsigned(7 downto 0) := (others => '1');
    variable g : unsigned(10 downto 0) := grow(ones, 3);
  begin
    assert is_zero(grow(zeros, n)) report "zeros not seen as zero" severity failure;
    assert not is_zero(grow(ones, n)) report "ones seen as zero" severity failure;

    -- The same against a value held in an object whose bounds it decided
    assert g /= (g'range => '0') report "ones matched all zeros" severity failure;
    assert g'length = 11 report "wrong length" severity failure;

    n := 0;
    assert is_zero(grow(zeros, n)) report "zeros of unchanged length" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
