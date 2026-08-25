-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- "wait for 0 ns" resumes in the next delta cycle, after the signal
-- assignments already made have taken effect (10.2, 14.7.5). It is not
-- Verilog's "#0", which resumes before those assignments land. And a
-- postponed statement runs only once the signals have settled (11.3).

use std.env.all;

entity t_vhdl_delta is
end entity;

architecture tb of t_vhdl_delta is
  signal x : integer := 5;
  signal y : integer := 0;
  signal zero : time := 0 ns;
begin

  -- Runs once the signals have settled, so it never sees 6 or 7, only the
  -- value each time step ended with. A postponed statement may not cause a
  -- delta cycle of its own, so it only reads.
  postponed assert x = 5 or x = 8
    report "postponed saw " & integer'image(x) severity failure;

  process
  begin
    -- A signal keeps its value until the delta ends
    assert x = 5 report "x at start" severity failure;
    x <= 6;
    assert x = 5 report "x changed too early" severity failure;

    wait for 0 ns;
    assert x = 6 report "x not updated after a delta" severity failure;

    -- Two deltas in a row, with no time passing
    x <= 7;
    y <= x;                         -- reads 6, the value before this delta
    wait for 0 ns;
    assert x = 7 report "x after second delta" severity failure;
    assert y = 6 report "y took the old x" severity failure;
    assert now = 0 ns report "time moved" severity failure;

    -- The same where the delay is not known until it is evaluated
    x <= 8;
    wait for zero;
    assert x = 8 report "x after a computed zero delay" severity failure;
    assert now = 0 ns report "time moved on a computed zero" severity failure;

    wait for 1 ns;

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
