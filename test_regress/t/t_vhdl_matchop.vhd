-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- The matching relational operators are predefined for BIT and for
-- STD_ULOGIC and for nothing else, and each gives back that same type
-- rather than a BOOLEAN -- which is what lets "?=" chain, and what
-- STD_LOGIC_1164 relies on where it leaves its own versions commented out.
-- The two equality ones are defined for a one-dimensional array of either
-- as well, and give back the element type (9.2.3).

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_matchop is
end entity;

architecture a of t_vhdl_matchop is
  -- Read through a signal so nothing is settled before it runs
  signal one : std_logic := '1';
begin
  process
    variable a : std_logic;
    variable r : std_logic;
    variable x : std_logic_vector(3 downto 0) := "1010";
    variable y : std_logic_vector(3 downto 0) := "1010";
    variable n : std_logic_vector(3 downto 0) := "1011";
    variable u : unsigned(3 downto 0) := "1010";
    variable w : unsigned(3 downto 0) := "0011";
    variable p : bit := '1';
    variable q : bit := '0';
    variable bv : bit_vector(3 downto 0) := "1100";
    variable bw : bit_vector(3 downto 0) := "1100";
    variable br : bit;
  begin
    wait for 1 ns;
    a := one;

    -- The result is a STD_LOGIC, so it can be compared with one
    r := a ?= '1';
    assert r = '1' report "scalar match" severity failure;
    assert (a ?= '0') = '0' report "scalar mismatch" severity failure;
    assert (a ?/= '0') = '1' report "scalar inequality" severity failure;

    -- and over a vector, where the answer is one element
    assert (x ?= y) = '1' report "vector match" severity failure;
    assert (x ?= n) = '0' report "vector mismatch" severity failure;
    assert (x ?/= n) = '1' report "vector inequality" severity failure;

    -- The ordering ones, over a scalar
    assert (std_logic'('0') ?< '1') = '1' report "?<" severity failure;
    assert (std_logic'('1') ?< '0') = '0' report "?< the other way" severity failure;
    assert (std_logic'('1') ?<= '1') = '1' report "?<=" severity failure;
    assert (std_logic'('1') ?> '0') = '1' report "?>" severity failure;
    assert (std_logic'('0') ?>= '1') = '0' report "?>=" severity failure;

    -- Giving back the operand's own type is what lets one chain
    assert ((a ?= '1') ?= '1') = '1' report "chained match" severity failure;

    -- BIT keeps its own type too
    br := p ?= q;
    assert br = '0' report "bit mismatch" severity failure;
    assert (p ?= p) = '1' report "bit match" severity failure;
    assert (bv ?= bw) = '1' report "bit_vector match" severity failure;
    assert (bv ?/= bw) = '0' report "bit_vector inequality" severity failure;

    -- numeric_std declares its own over UNSIGNED, giving a STD_LOGIC
    assert (u ?> w) = '1' report "unsigned ?>" severity failure;
    assert (u ?< w) = '0' report "unsigned ?<" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
