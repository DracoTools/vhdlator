-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A simulation cycle updates every signal whose transaction is due before it
-- resumes the processes whose timeouts have expired (14.7.5.1), so a process
-- that waits a nanosecond sees what an "after 1 ns" assignment delivered at
-- that instant. 'DELAYED is the same signal moved along in time.

library ieee;
use ieee.numeric_std.all;
use ieee.std_logic_1164.all;
use std.env.all;

entity t_vhdl_delayed is
end entity;

architecture tb of t_vhdl_delayed is
  signal x : natural := 0;
  signal y : natural := 0;
  signal a : unsigned(31 downto 0) := (others => '0');
  signal b : unsigned(7 downto 0) := x"10";
begin

  x <= 1 after 1 ns;
  y <= 1 after 1 ns, 2 after 2 ns, 3 after 3 ns;

  main : process is
  begin
    -- The transaction due at this instant has been delivered
    wait for 1 ns;
    assert x = 1 report "x1" severity failure;
    assert y = 1 report "y1" severity failure;
    -- 'DELAYED with no time is the value one delta back, which during the
    -- cycle of an event is the value before it
    assert x'delayed = 0 report "d0" severity failure;
    wait for 0 ns;
    assert x'delayed = 1 report "d1" severity failure;

    wait for 1 ns;
    assert y = 2 report "y2" severity failure;
    assert y'delayed(1 ns) = 1 report "dy1" severity failure;
    wait for 1 ns;
    assert y = 3 report "y3" severity failure;
    assert y'delayed(1 ns) = 2 report "dy2" severity failure;

    -- A physical literal is the largest integer not greater than the product
    -- of its abstract literal and its unit (5.2.4.1), worked out exactly
    assert 8.2 ms = 8200 us report "p1" severity failure;
    assert 1.5 ns = 1500 ps report "p2" severity failure;
    assert 0.001 ns = 1 ps report "p3" severity failure;

    -- numeric_std arithmetic between two lengths resizes to the longer
    a <= a + b;
    wait for 1 ns;
    assert a = 16 report "n1" severity failure;

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
