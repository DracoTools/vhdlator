-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A '-' stands for a place that matches whatever it is compared with
-- (9.2.3, 16.8.3) -- which is what a design writes to decode an address
-- against a pattern.  Nothing carries a don't-care once the metavalues
-- have collapsed, so a pattern settled before the design runs is read
-- while it is still a value of its own type.  A pattern only running
-- decides cannot be, and is an accepted divergence.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_dontcare is
end entity;

architecture a of t_vhdl_dontcare is
  -- Read through a signal so nothing is settled before it runs
  signal src : std_logic_vector(7 downto 0) := "10100110";
  signal one : std_logic := '1';
  constant c_pat : std_logic_vector(7 downto 0) := "1010----";
begin
  process
    variable v : std_logic_vector(7 downto 0);
    variable b : std_logic;
    variable u : unsigned(7 downto 0);
  begin
    wait for 1 ns;
    v := src;
    b := one;
    u := unsigned(src);

    -- STD_MATCH against a pattern, which is what it is for
    assert std_match(v, "1010----") report "a pattern that matches" severity failure;
    assert not std_match(v, "1011----") report "a pattern that does not" severity failure;
    assert std_match(v, "--------") report "a pattern of all don't-cares" severity failure;
    assert std_match(v, "10100110") report "a pattern with no don't-care" severity failure;
    assert not std_match(v, "10100111") report "a pattern one place out" severity failure;

    -- and through a constant rather than a literal
    assert std_match(v, c_pat) report "a pattern held in a constant" severity failure;

    -- The matching operators, whose result is a STD_LOGIC
    assert (v ?= "1010----") = '1' report "?= a pattern" severity failure;
    assert (v ?= "1011----") = '0' report "?= a pattern that does not" severity failure;
    assert (v ?/= "1010----") = '0' report "?/= a pattern" severity failure;

    -- One element, where the whole pattern is the don't-care
    assert (b ?= '-') = '1' report "a scalar don't-care" severity failure;
    assert (b ?/= '-') = '0' report "a scalar don't-care, inverted" severity failure;

    -- An UNSIGNED reaches the same operators
    assert std_match(u, "1010----") report "unsigned against a pattern" severity failure;

    -- and the ordinary equality is untouched by any of it
    assert v = "10100110" report "plain equality" severity failure;
    assert v /= "10100111" report "plain inequality" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
