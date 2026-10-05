-- DESCRIPTION: Verilator: VHDL 'VALUE and 'IMAGE of physical values
--
-- "time'value(\" -1.5 ns \")" is -1500 ps: the abstract part of a physical
-- literal may be a decimal literal with a point or an exponent, and the
-- value counts in 64 bits whatever the unit (15.5.2, 5.2.4). The image of
-- a physical value is its count of the primary unit, below zero as well,
-- followed by that unit's name -- "um" for a type of the design's own, not
-- the femtoseconds of TIME (16.2.3; GHDL gna bug7751).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

use std.textio.all;

entity t_vhdl_physimg is
end entity;

architecture a of t_vhdl_physimg is
  type distance is range -1000000 to 1000000 units
    um;
    mm = 1000 um;
  end units;
  function t_val(t : string) return time is
  begin
    return time'value(t);
  end function;
  function i_val(t : string) return integer is
  begin
    return integer'value(t);
  end function;
  signal s : time := -3 ns;
begin
  process is
    variable v : time := -2 ns;
    variable d : distance := -3 mm;
    variable l : line;
  begin
    assert t_val("1 ns") = 1 ns severity failure;
    assert t_val("-1 ns") = -1 ns severity failure;
    assert t_val("1.5 ns") = 1500 ps severity failure;
    assert t_val(" -1.5 ns ") = -1500 ps severity failure;
    assert t_val("2.25 us") = 2250 ns severity failure;
    assert t_val("1e3 ps") = 1 ns severity failure;
    assert t_val("3 ms") = 3000 us severity failure;
    assert t_val("2 sec") = 2000 ms severity failure;
    assert t_val("1 hr") = 60 min severity failure;
    assert t_val("1_000 ns") = 1 us severity failure;
    assert t_val("2.5e-3 us") = 2500 ps severity failure;
    assert i_val("1e3") = 1000 severity failure;
    assert i_val(" -42 ") = -42 severity failure;
    assert i_val("1_234") = 1234 severity failure;
    assert time'image(v) = "-2000000 fs" severity failure;
    assert time'image(s) = "-3000000 fs" severity failure;
    assert to_string(v - 1 ns) = "-3000000 fs" severity failure;
    assert time'image(4 ns) = "4000000 fs" severity failure;
    assert distance'image(d) = "-3000 um" severity failure;
    assert to_string(d + 5 mm) = "2000 um" severity failure;
    write(l, v);
    assert l.all = "-2 ns" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
