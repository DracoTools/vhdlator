-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Nothing about an array of logic says that its leftmost element is a
-- sign, so every operator of numeric_std that reads a SIGNED as a number
-- has to be told: the comparisons, division, the remainder, the product
-- and the arithmetic shift (9.3.6, 16.5).  Untold, each reads the
-- positive number the same elements spell.  The UNSIGNED cases beside
-- them must not change.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_signedop is
end entity;

architecture a of t_vhdl_signedop is
  -- Read through a signal so nothing is settled before it runs
  signal three : integer := 3;
begin
  process
    variable a  : signed(7 downto 0);
    variable b  : signed(7 downto 0);
    variable c  : signed(11 downto 0);
    variable ua : unsigned(7 downto 0);
    variable ub : unsigned(7 downto 0);
    variable uc : unsigned(11 downto 0);
    variable p  : signed(15 downto 0);
    variable q  : unsigned(15 downto 0);
  begin
    wait for 1 ns;
    a  := to_signed(-10, 8);
    b  := to_signed(three, 8);
    c  := to_signed(5, 12);
    ua := to_unsigned(246, 8);
    ub := to_unsigned(three, 8);
    uc := to_unsigned(5, 12);

    -- Below zero, and away from it
    assert abs a = to_signed(10, 8) report "abs of a negative" severity failure;
    assert abs b = to_signed(3, 8) report "abs of a positive" severity failure;
    assert -a = to_signed(10, 8) report "negation" severity failure;

    -- The comparisons, at one length
    assert a < b report "-10 is not less than 3" severity failure;
    assert not (a > b) report "-10 is greater than 3" severity failure;
    assert a <= b report "-10 is not at most 3" severity failure;
    assert not (a >= b) report "-10 is at least 3" severity failure;
    assert a /= b report "-10 equals 3" severity failure;

    -- and across two, which numeric_std compares by value
    assert a < c report "-10 is not less than 5" severity failure;
    assert not (c < a) report "5 is less than -10" severity failure;

    -- and against an integer, which is compared by value too (16.5)
    assert a < 3 report "-10 is not less than the integer 3" severity failure;
    assert a > -20 report "-10 is not greater than the integer -20" severity failure;
    assert a = -10 report "-10 does not equal the integer -10" severity failure;

    -- Division and what it leaves
    assert a / b = to_signed(-3, 8) report "signed division" severity failure;
    assert (a rem b) = to_signed(-1, 8) report "signed remainder" severity failure;
    assert (a mod b) = to_signed(2, 8) report "signed modulus" severity failure;

    -- A product is as long as its two operands together, sign and all
    p := a * b;
    assert p = to_signed(-30, 16) report "signed product" severity failure;

    -- The right shift of a SIGNED brings its sign down with it
    assert shift_right(a, 1) = to_signed(-5, 8) report "arithmetic shift" severity failure;

    -- None of which may touch the UNSIGNED beside it
    assert not (ua < ub) report "246 is less than 3" severity failure;
    assert ua > ub report "246 is not greater than 3" severity failure;
    assert ua / ub = to_unsigned(82, 8) report "unsigned division" severity failure;
    assert (ua rem ub) = to_unsigned(0, 8) report "unsigned remainder" severity failure;
    q := ua * ub;
    assert q = to_unsigned(738, 16) report "unsigned product" severity failure;
    assert shift_right(ua, 1) = to_unsigned(123, 8) report "logical shift" severity failure;
    assert not (ua = uc) report "246 equals 5" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
