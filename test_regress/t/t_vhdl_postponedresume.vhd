-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A postponed process that waits resumes at the end of the time step its wait
-- ends in, once the last delta cycle has run (10.3, 14.7.5.3), and sees the
-- next change it waits on (NVC wait12)

entity t_vhdl_postponedresume is
end entity;

architecture a of t_vhdl_postponedresume is
  signal x, y : integer := 0;
  signal wakes : integer := 0;
begin
  stim : process is
  begin
    wait for 1 ns;
    x <= 1;
    wait for 0 ns;
    x <= 2;
    wait for 0 ns;
    x <= 3;
    wait for 1 ns;
    x <= 4;
    wait on y;
    assert now = 3 ns and y = 2 severity failure;
    assert wakes = 2 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;

  post : postponed process is
    variable n : integer := 0;
  begin
    wait on x;
    -- Three deltas changed x in this step; the process runs after the last
    assert now = 1 ns and x = 3 severity failure;
    n := n + 1;
    wakes <= n after 1 fs;
    wait on x;
    assert now = 2 ns and x = 4 severity failure;
    n := n + 1;
    wakes <= n after 1 fs;
    y <= 2 after 1 ns;
    wait;
  end process;
end architecture;
