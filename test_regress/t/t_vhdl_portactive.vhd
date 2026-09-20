-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A transaction is not an event: assigning a signal the value it already
-- holds is a transaction and nothing else (14.7.3). Nothing about it crosses
-- a port association by itself, since the port's value does not change, so
-- 'ACTIVE and 'TRANSACTION asked of a port are answered by what the actual
-- did rather than by what the port's value did.

entity t_vhdl_portactive_child is
  port (p : in integer; hits : out integer);
end entity;

architecture rtl of t_vhdl_portactive_child is
  signal n : integer := 0;
begin
  -- A process sensitive to the port itself wakes on an event, which a
  -- transaction carrying the same value is not: it is 'TRANSACTION, a signal
  -- that flips on every transaction, that sees them all (16.2)
  watch: process (p'transaction) is
  begin
    if now > 0 ns then
      assert p'active report "the port saw no transaction" severity failure;
      n <= n + 1;
    end if;
  end process;
  hits <= n;
end architecture;

use std.env.all;

entity t_vhdl_portactive is
end entity;

architecture tb of t_vhdl_portactive is
  component t_vhdl_portactive_child is
    port (p : in integer; hits : out integer);
  end component;
  signal s : integer := 7;
  signal hits : integer := 0;
begin

  dut: t_vhdl_portactive_child port map (p => s, hits => hits);

  main: process is
  begin
    wait for 1 ns;
    assert not s'active report "the actual was active before anything wrote it"
      severity failure;

    -- The same value again: a transaction, and no event. It is the cycle in
    -- which the transaction lands that 'ACTIVE speaks of, which is the one a
    -- "wait for 0 ns" resumes in (14.7.3).
    s <= 7;
    wait for 0 ns;
    assert s'active report "the actual saw no transaction" severity failure;
    assert not s'event report "writing the same value was an event" severity failure;

    -- ... and a different one, which is both
    s <= 8;
    wait for 0 ns;
    assert s'active report "the actual saw no transaction" severity failure;
    assert s'event report "a new value was no event" severity failure;
    wait for 1 ns;

    -- The child woke for both of them, the unchanged value included
    assert hits = 2 report "the child saw " & integer'image(hits)
      & " transaction(s) of two" severity failure;
    stop(0);
  end process;

end architecture;
