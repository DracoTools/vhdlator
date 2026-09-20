-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A loop runs over the values of its range and no others (10.10), including
-- where the range ends at the last value the type holds: stepping past that
-- value wraps round to the first, so a test written before each iteration
-- would be true again and the loop would never end.

use std.env.all;

entity t_vhdl_loopend is
end entity;

architecture tb of t_vhdl_loopend is
begin
  main: process is
    variable n : integer;
    variable c : character;
  begin
    n := 0;
    for i in integer'high to integer'high loop
      n := n + 1;
    end loop;
    assert n = 1 report "a loop ending at INTEGER'HIGH ran " & integer'image(n)
      & " time(s)" severity failure;

    n := 0;
    for i in integer'low downto integer'low loop
      n := n + 1;
    end loop;
    assert n = 1 report "a loop ending at INTEGER'LOW ran " & integer'image(n)
      & " time(s)" severity failure;

    -- A null range at the extreme runs no times at all
    n := 0;
    for i in integer'high to integer'high - 1 loop
      n := n + 1;
    end loop;
    assert n = 0 report "a null range at INTEGER'HIGH ran" severity failure;

    -- The same at the end of an enumeration, which already worked
    n := 0;
    for ch in character'high downto character'high loop
      n := n + 1;
      c := ch;
    end loop;
    assert n = 1 report "a loop ending at CHARACTER'HIGH ran " & integer'image(n)
      & " time(s)" severity failure;
    assert c = character'high report "the parameter was not the last value"
      severity failure;

    -- An ordinary loop is untouched
    n := 0;
    for i in 1 to 5 loop
      n := n + i;
    end loop;
    assert n = 15 report "an ordinary loop changed" severity failure;
    stop(0);
  end process;
end architecture;
