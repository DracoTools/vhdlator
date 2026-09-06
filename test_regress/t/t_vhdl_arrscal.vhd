-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- VHDL-2008 gives the logical operators a form with one array operand and
-- one of the element type, and the scalar stands for every element of the
-- array (9.2.2).  It is repeated across the array's length, not widened
-- into it -- which would put it in the rightmost element and leave the
-- rest zero, and reads correctly for '0' by coincidence.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_arrscal is
end entity;

architecture a of t_vhdl_arrscal is
  -- Read through a signal so nothing is settled before it runs
  signal one : std_logic := '1';
begin
  process
    variable v : std_logic_vector(3 downto 0) := "1010";
    variable b : std_logic;
    variable w : bit_vector(3 downto 0) := "1100";
    variable c : bit := '1';
    variable u : std_logic_vector(0 to 3) := "1010";
  begin
    wait for 1 ns;
    b := one;

    -- The scalar on either side, and every operator that takes the form
    assert (v xor b) = "0101" report "xor with a scalar" severity failure;
    assert (b xor v) = "0101" report "scalar on the left" severity failure;
    assert (v and b) = "1010" report "and with a scalar" severity failure;
    assert (v or b) = "1111" report "or with a scalar" severity failure;
    assert (v nand b) = "0101" report "nand with a scalar" severity failure;
    assert (v nor b) = "0000" report "nor with a scalar" severity failure;
    assert (v xnor b) = "1010" report "xnor with a scalar" severity failure;

    -- A zero reads the same widened as repeated, which is why it hid this
    b := '0';
    assert (v xor b) = "1010" report "xor with a zero scalar" severity failure;
    assert (v and b) = "0000" report "and with a zero scalar" severity failure;
    assert (v or b) = "1010" report "or with a zero scalar" severity failure;

    -- The same over BIT, whose vectors are a different array type
    assert (w xor c) = "0011" report "bit_vector with a scalar" severity failure;
    assert (w and c) = "1100" report "bit_vector and a scalar" severity failure;

    -- and over an array indexed the other way
    b := one;
    assert (u xor b) = "0101" report "ascending array with a scalar" severity failure;

    -- Two arrays are still element by element
    assert (v xor "1111") = "0101" report "two arrays" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
