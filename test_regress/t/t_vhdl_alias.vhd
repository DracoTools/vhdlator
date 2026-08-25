-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An alias is another name for what it denotes (6.6), including a slice or
-- an element of an object, and writing through it writes the object.

library ieee;
use ieee.std_logic_1164.all;
use std.env.all;

entity t_vhdl_alias is
end entity;

architecture tb of t_vhdl_alias is

  signal v : std_logic_vector(7 downto 0) := "10110001";
  alias whole : std_logic_vector(7 downto 0) is v;
  alias top   : std_logic_vector(7 downto 4) is v(7 downto 4);

  -- An alias may number its elements its own way (6.6.2)
  alias hi    : std_logic_vector(3 downto 0) is v(7 downto 4);
  alias lo    : std_logic_vector(0 to 3)     is v(3 downto 0);

  signal n : integer := 42;
  alias m : integer is n;

  type state_t is (idle, busy, done);
  alias st is state_t;

begin

  process
    variable s : st;
  begin
    assert whole = "10110001" report "whole" severity failure;
    assert top = "1011" report "top" severity failure;
    assert whole(3) = '0' report "an element of an alias" severity failure;
    assert m = 42 report "m" severity failure;

    -- hi(0) is v(4) and lo(0) is v(3), whichever way each range runs
    assert hi = "1011" report "hi" severity failure;
    assert hi(3) = '1' and hi(0) = '1' and hi(2) = '0' report "hi element" severity failure;
    assert lo(0) = '0' and lo(3) = '1' report "lo element" severity failure;
    assert lo(1 to 2) = "00" report "lo slice" severity failure;

    -- An alias of a type is that type, and its literals come with it
    s := busy;
    assert s = busy report "alias of a type" severity failure;

    -- Writing through an alias writes what it denotes
    whole(0) <= '1';
    wait for 1 ns;
    assert v(0) = '1' report "write through an alias" severity failure;
    hi(2) <= '1';
    wait for 1 ns;
    assert v(6) = '1' report "write through a renumbered alias" severity failure;

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
