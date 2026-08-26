-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- TO_STRING is an ordinary function, not only something a report is built
-- from: for a scalar it gives what T'IMAGE gives, except that a CHARACTER or
-- a STRING is its own characters (16.3). JUSTIFY pads one out to a width
-- (16.4). And a loop over the range of a string whose length is not fixed by
-- its subtype runs from 1 to however long the value is.

library ieee;
use ieee.std_logic_1164.all;

use std.env.all;
use std.textio.all;

entity t_vhdl_tostring is
end entity;

architecture tb of t_vhdl_tostring is

  type colour is (red, green, blue);

  function lower (s : string) return string is
    variable r : string(s'range);
  begin
    for i in s'range loop
      if s(i) >= 'A' and s(i) <= 'Z' then
        r(i) := character'val(character'pos(s(i)) + 32);
      else
        r(i) := s(i);
      end if;
    end loop;
    return r;
  end function;

begin

  main : process is
    variable n : integer := 42;
    variable b : bit := '1';
    variable c : colour := green;
    variable t : time := 3 ns;
    variable v8 : std_logic_vector(7 downto 0) := "10100101";
    variable v5 : std_logic_vector(4 downto 0) := "10101";
    variable v4 : std_logic_vector(3 downto 0) := "1100";
    variable unit : time := ns;
    variable l : line;
  begin
    assert to_string(n) = "42" report "integer" severity failure;
    assert to_string(b) = "1" report "bit" severity failure;
    assert to_string(c) = "green" report "enumeration" severity failure;
    assert to_string('x') = "x" report "character" severity failure;
    assert to_string(t, ns) = "3 ns" report "time in a unit" severity failure;
    assert to_string(t, unit) = "3 ns" report "time in a unit read at run time"
      severity failure;

    assert justify(string'("ab"), right, 5) = "   ab" report "justify right" severity failure;
    assert justify(string'("ab"), left, 5) = "ab   " report "justify left" severity failure;
    assert justify(string'("abcdef"), right, 3) = "abcdef" report "justify too narrow"
      severity failure;

    -- The length of what a LINE points at is only known while running
    l := new string'("MiXeD");
    assert lower(l.all) = "mixed" report "loop over a dynamic range" severity failure;
    deallocate(l);

    -- The radix forms write the value in that base, padded to whole digits
    -- (16.3). Written as a conversion rather than compiled from the IEEE
    -- body, which pads through an array that is null when the length
    -- divides evenly.
    assert to_hstring(v8) = "A5" report "to_hstring: " & to_hstring(v8) severity failure;
    assert to_hstring(v5) = "15" report "to_hstring of five bits" severity failure;
    assert to_ostring(v8) = "245" report "to_ostring: " & to_ostring(v8) severity failure;
    assert to_bstring(v4) = "1100" report "to_bstring" severity failure;

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
