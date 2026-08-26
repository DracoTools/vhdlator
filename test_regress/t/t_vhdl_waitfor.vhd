-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- "wait until C for T" resumes on whichever of the two comes first (10.2).
-- The condition is still only tested on an event, so a condition that is
-- already true when the statement is reached does not stop the timeout.

library ieee;
use ieee.std_logic_1164.all;
use std.env.all;

entity t_vhdl_waitfor is
end entity;

architecture tb of t_vhdl_waitfor is
  signal ack : std_logic := '0';
  signal trig : std_logic := '0';
  signal vec : std_logic_vector(0 to 3) := "0000";
begin

  process
  begin
    wait for 30 ns;
    ack <= '1';
    wait;
  end process;

  process
    variable t0 : time;
  begin
    -- The timeout wins: ack does not rise inside 10 ns
    t0 := now;
    wait until ack = '1' for 10 ns;
    assert now - t0 = 10 ns report "timeout did not end the wait" severity failure;
    assert ack = '0' report "ack rose early" severity failure;

    -- The condition wins: ack rises at 30 ns, inside the 100 ns allowed
    wait until ack = '1' for 100 ns;
    assert now = 30 ns report "condition did not end the wait" severity failure;

    -- Already true, but the statement still suspends, so the timeout fires
    t0 := now;
    wait until ack = '1' for 5 ns;
    assert now - t0 = 5 ns report "an already true condition returned early"
      severity failure;

    -- With an explicit sensitivity list, and a condition that never holds
    t0 := now;
    wait on ack until ack = '0' for 7 ns;
    assert now - t0 = 7 ns report "wait on/until/for" severity failure;

    -- A sensitivity list with a timeout and no condition: whichever comes
    -- first ends the wait
    t0 := now;
    wait on ack for 9 ns;
    assert now - t0 = 9 ns report "wait on/for timeout" severity failure;

    -- And where the signal changes first, it is what ends it
    t0 := now;
    trig <= '1' after 4 ns;
    wait on trig for 40 ns;
    assert now - t0 = 4 ns
      report "wait on/for woke at " & time'image(now - t0) severity failure;

    -- An element of a signal is watched on its own
    t0 := now;
    vec <= "0100" after 3 ns;
    wait on vec(1) for 40 ns;
    assert now - t0 = 3 ns
      report "wait on an element woke at " & time'image(now - t0) severity failure;

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
