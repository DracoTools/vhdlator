-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- The signal attributes of 16.2.4 that separate a transaction from an
-- event: a driver that assigns the value a signal already holds makes the
-- signal active without changing it. Before anything at all has happened,
-- the "how long ago" attributes answer TIME'HIGH.

use std.env.all;

entity t_vhdl_sigattr is
end entity;

architecture tb of t_vhdl_sigattr is
  signal x : integer := 5;
  signal b : bit := '0';
  signal z : bit;                       -- Never driven
  signal c : integer := 0;
begin

  -- A concurrent assignment is a process, so it makes a transaction on c
  -- at time zero and again whenever what it reads changes
  c <= x + 1;

  main : process is
    variable t0 : bit;
  begin
    -- Nothing has happened yet
    assert x'last_event = time'high report "le0" severity failure;
    assert x'last_active = time'high report "la0" severity failure;
    assert z'last_event = time'high report "lez" severity failure;
    assert z'last_active = time'high report "laz" severity failure;
    assert x'stable report "st0" severity failure;
    assert x'stable(1 hr) report "sth" severity failure;
    assert x'quiet(1 hr) report "quh" severity failure;
    assert not x'event report "ev0" severity failure;
    assert not x'active report "ac0" severity failure;
    assert x'last_value = 5 report "lv0" severity failure;

    x <= 0;
    -- Not delivered yet, so nothing has changed
    assert x'last_value = 5 report "lv1" severity failure;
    assert not x'event report "ev1" severity failure;

    wait for 1 ns;
    assert x = 0 report "x1" severity failure;
    assert x'last_value = 5 report "lv2" severity failure;
    assert x'last_event = 1 ns report "le1" severity failure;
    assert x'last_active = 1 ns report "la1" severity failure;
    assert x'stable(1 ns) report "st1" severity failure;
    assert not x'stable(2 ns) report "st2" severity failure;

    -- Assigning the value it already holds is a transaction, not an event
    t0 := x'transaction;
    x <= 0;
    wait for 0 ns;
    assert not x'event report "ev2" severity failure;
    assert x'active report "ac2" severity failure;
    assert x'transaction /= t0 report "tr1" severity failure;
    assert x'last_active = 0 ns report "la2" severity failure;
    assert x'last_event = 1 ns report "le2" severity failure;
    assert not x'quiet report "qu2" severity failure;
    assert x'stable report "st3" severity failure;

    wait for 0 ns;
    assert not x'active report "ac3" severity failure;
    assert x'quiet report "qu3" severity failure;

    -- b is never assigned at all, so it stays quiet for ever
    assert b'quiet(1 hr) report "qub" severity failure;
    assert b'last_active = time'high report "lab" severity failure;

    -- The concurrent assignment ran at time zero, so c is not untouched
    assert c = 1 report "c0" severity failure;
    assert c'last_active <= 1 ns report "lac" severity failure;

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
