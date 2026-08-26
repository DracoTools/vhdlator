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
  -- A vector and an integer, to show that sensitivity means "changes",
  -- not "an edge of the least significant bit"
  signal v : std_logic_vector(3 downto 0) := "0000";
  signal n : integer := 0;
  signal vseen, nseen : integer := 0;
  signal seen : integer := 0;
  signal texts : string(1 to 40) := (others => ' ');
  -- VHDL-2008 gives BIT and BOOLEAN edge functions of their own (16.3)
  signal b : bit := '0';
  signal k : boolean := false;
  signal bseen, kseen : integer := 0;
  type down is range 10 downto 0;

begin

  -- A process with a sensitivity list, which is not a clocked one
  watcher : process (s)
  begin
    if s'event then
      seen <= seen + 1;
    end if;
  end process;

  edges : process (b, k)
  begin
    if rising_edge(b) then bseen <= bseen + 1; end if;
    if falling_edge(k) then kseen <= kseen + 1; end if;
  end process;

  wide : process (v, n)
  begin
    vseen <= vseen + 1;
    nseen <= nseen + 1;
  end process;

  driver : process
  begin
    wait for 10 ns;
    s <= '1';
    b <= '1';
    k <= true;
    v <= "0010";   -- bit 1, not bit 0
    n <= 2;        -- bit 1, not bit 0
    wait for 10 ns;
    s <= '0';
    b <= '0';
    k <= false;
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
    -- One run at the start, and one for the change, which is not on bit 0
    assert vseen = 2 report "vseen " & integer'image(vseen) severity failure;
    assert nseen = 2 report "nseen " & integer'image(nseen) severity failure;

    wait for 10 ns;
    assert s = '0' report "s later" severity failure;
    assert s'last_value = '1' report "last_value later" severity failure;
    assert s'last_event = 5 ns report "last_event later" severity failure;
    assert seen = 2 report "seen later" severity failure;
    assert bseen = 1 report "rising_edge of a bit" severity failure;
    assert kseen = 1 report "falling_edge of a boolean" severity failure;

    -- 'SUCC and 'PRED are about the type's own order; 'LEFTOF and 'RIGHTOF
    -- are about the range's, and turn round with it (16.2.3)
    assert integer'succ(4) = 5 report "succ" severity failure;
    assert integer'pred(4) = 3 report "pred" severity failure;
    assert integer'leftof(4) = 3 report "leftof of an ascending range" severity failure;
    assert integer'rightof(4) = 5 report "rightof of an ascending range" severity failure;
    assert down'succ(4) = 5 report "succ of a descending range" severity failure;
    assert down'pred(4) = 3 report "pred of a descending range" severity failure;
    assert down'leftof(4) = 5 report "leftof of a descending range" severity failure;
    assert down'rightof(4) = 3 report "rightof of a descending range" severity failure;
    assert down'left = 10 and down'right = 0 report "descending bounds" severity failure;
    assert not down'ascending report "descending" severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
