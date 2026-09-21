-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- The bit that records a signal's transactions is flipped by reading it, and
-- a concurrent statement waits on what it reads (11.6). A guarded assignment
-- carries that flip inside its own process, so the process was woken by the
-- very mark it made and ran again for each one, for ever. A design that names
-- S'TRANSACTION itself still waits on the bit, since that read stands outside
-- any assignment to it.

use std.env.all;

entity t_vhdl_guardquiet is
end entity;

architecture tb of t_vhdl_guardquiet is
  signal gate : bit := '0';
  signal din : bit := '0';
  signal pass, latch : bit;
  signal seen : integer := 0;
begin

  gate <= '1' after 10 ns, '0' after 30 ns;
  din <= '1' after 5 ns, '0' after 20 ns, '1' after 40 ns;

  b: block (gate = '1')
  begin
    pass <= din;
    latch <= guarded din;
  end block;

  -- Waiting on the transaction bit by name is a wait the design wrote, and
  -- it still works: every transaction on "pass" wakes this
  watch: process (pass'transaction) is
  begin
    if now > 0 ns then seen <= seen + 1; end if;
  end process;

  main: process is
  begin
    wait for 15 ns;
    -- The guard went true at 10 ns, so the guarded driver took din's value
    assert latch = '1' report "the guarded driver never connected" severity failure;
    assert pass = '1' report "the plain assignment did not follow" severity failure;

    wait for 10 ns;  -- 25 ns: din fell at 20 ns while the guard was still true
    assert latch = '0' report "the guarded driver did not follow" severity failure;

    wait for 20 ns;  -- 45 ns: the guard fell at 30 ns, so 40 ns did not reach it
    assert latch = '0' report "a disconnected driver still drove" severity failure;
    assert pass = '1' report "the plain assignment stopped following" severity failure;
    assert latch'quiet report "the latch was not quiet" severity failure;

    -- din made three transactions, and "pass" followed each of them
    assert seen = 3 report "the transaction bit woke " & integer'image(seen)
      & " times of three" severity failure;

    report "*-* All Finished *-*" severity note;
    stop(0);
  end process;

end architecture;
