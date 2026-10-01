-- DESCRIPTION: Verilator: VHDL user-defined attribute values, indexed and bounded
--
-- This file is part of Verilator.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
--
-- A user-defined attribute stands for the value its specification gave
-- (7.2), and is indexed and asked its bounds as that value is. Written as a
-- string literal for a BIT_VECTOR, "101", an element read 0 wherever it was:
-- the character's code was taken for the element. And 'LEFT of the value
-- was refused. NVC's attr11; nvc 1.22.1 and Questa 2025.2 pass this test.
-- GHDL 7.0.0-dev refuses 'LEFT of a value that is not an object, unless
-- -frelaxed; the other two accept it.

entity t_vhdl_userattrval is
end entity;

architecture test of t_vhdl_userattrval is
begin
  process is
    variable i : integer;
    attribute a : bit_vector;
    attribute a of i : variable is "101";
    attribute s : string;
    attribute s of i : variable is "hello";
    subtype nibble_t is bit_vector(3 downto 0);
    attribute d : nibble_t;
    attribute d of i : variable is "0011";
  begin
    assert i'a = "101" report "whole" severity failure;
    assert i'a(0) = '1' and i'a(1) = '0' and i'a(2) = '1' report "elements" severity failure;
    assert i'a'left = 0 and i'a'right = 2 and i'a'length = 3 and i'a'ascending
      report "bounds" severity failure;
    -- An attribute of a constrained type is indexed by its own range
    assert i'd(3) = '0' and i'd(0) = '1' and i'd'left = 3 report "constrained" severity failure;
    -- Text, as it was before
    assert i's(2) = 'e' and i's'left = 1 and i's'length = 5 report "string" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
