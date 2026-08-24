-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- The signal attributes of 16.2.2: what has happened to a signal, rather
-- than what its type is. A compiled model has no value history, so what
-- they ask about is recorded as it happens.

library ieee;
use ieee.std_logic_1164.all;
use std.env.all;

entity t_vhdl_attr is
end entity;

architecture tb of t_vhdl_attr is
  signal s : std_logic := '0';
  signal seen : integer := 0;
  signal texts : string(1 to 40) := (others => ' ');
begin

  -- A process with a sensitivity list, which is not a clocked one
  watcher : process (s)
  begin
    if s'event then
      seen <= seen + 1;
    end if;
  end process;

  driver : process
  begin
    wait for 10 ns;
    s <= '1';
    wait for 10 ns;
    s <= '0';
    wait;
  end process;

  checker : process
  begin
    -- Before anything happens, nothing has changed
    assert not s'event report "event at time zero" severity failure;
    assert s'stable report "not stable at time zero" severity failure;

    wait for 15 ns;
    assert s = '1' report "s" severity failure;
    assert not s'event report "event between changes" severity failure;
    assert s'stable report "not stable between changes" severity failure;
    assert s'last_value = '0' report "last_value" severity failure;
    assert s'last_event = 5 ns
      report "last_event " & time'image(s'last_event) severity failure;
    assert seen = 1 report "seen " & integer'image(seen) severity failure;

    wait for 10 ns;
    assert s = '0' report "s later" severity failure;
    assert s'last_value = '1' report "last_value later" severity failure;
    assert s'last_event = 5 ns report "last_event later" severity failure;
    assert seen = 2 report "seen later" severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
