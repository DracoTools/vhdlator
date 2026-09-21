-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A physical type runs from its own LOW, which is negative, so a value of
-- one is signed. Emitted as an unsigned sixty-four bits, "-0.01 ns" is the
-- largest value there is rather than a small negative one, and every
-- comparison against it answers backwards -- except where both sides fold,
-- which is what kept it out of sight.

use std.env.all;

entity t_vhdl_negtime is
end entity;

architecture tb of t_vhdl_negtime is
  signal ss : time := 263996700 fs;
begin

  main: process is
    constant zero : time := 0 fs;
    variable diff : time;
  begin
    -- The literal itself is right either way
    assert -0.01 ns = -10000 fs report "the literal is " & time'image(-0.01 ns)
      severity failure;

    -- Against something only lowering builds, rather than two folded values
    diff := ss - 3 * 11 * 7.9999 ns;
    assert diff = 0 fs report "the difference is " & time'image(diff) severity failure;
    assert -0.01 ns < diff report "a negative time was not below zero" severity failure;
    assert diff > -0.01 ns report "zero was not above a negative time" severity failure;
    assert diff < 0.01 ns report "zero was not below a positive time" severity failure;

    -- ... and of a signal, which is neither folded nor a variable
    assert -1 ns < ss report "a negative time was not below a signal" severity failure;
    assert ss > -1 ns report "a signal was not above a negative time" severity failure;

    -- Both sides folded, which always worked
    assert -0.01 ns < zero report "the folded pair went wrong too" severity failure;

    -- Ordering among negatives
    assert -2 ns < -1 ns report "two negative times compared backwards" severity failure;

    report "*-* All Finished *-*" severity note;
    stop(0);
  end process;

end architecture;
