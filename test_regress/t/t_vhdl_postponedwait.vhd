-- DESCRIPTION: Verilator: VHDL postponed process that waits on its signals,
-- and one sensitive to a signal wider than a bit
--
-- A postponed process runs at the end of the last delta of a time step in
-- which what it waits for happened (10.3, 14.7.5.3). One whose first
-- statement is "wait on i, j" runs nothing at the start; one with a
-- sensitivity list runs once at the start. Either sees the values the step
-- ended with, in the step they changed in -- for an INTEGER as for a BIT
-- (GHDL gna issue2536).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_postponedwait is
end entity;

architecture test of t_vhdl_postponedwait is
  signal i : integer := 0;
  signal j : integer := 0;
begin
  i <= 1 after 1 ns, 2 after 2 ns, 3 after 4 ns;
  -- Two deltas in a row at 2 ns: j follows i
  j <= i;

  waiter : postponed process is
    variable n : integer := 0;
  begin
    wait on i, j;
    n := n + 1;
    case n is
      when 1 => assert now = 1 ns and i = 1 and j = 1 severity failure;
      when 2 => assert now = 2 ns and i = 2 and j = 2 severity failure;
      when 3 => assert now = 4 ns and i = 3 and j = 3 severity failure;
      when others => assert false severity failure;
    end case;
    report "waiter run " & integer'image(n);
  end process;

  listed : postponed process (i) is
    variable n : integer := 0;
  begin
    n := n + 1;
    case n is
      when 1 => assert now = 0 ns and i = 0 severity failure;
      when 2 => assert now = 1 ns and i = 1 severity failure;
      when 3 => assert now = 2 ns and i = 2 severity failure;
      when 4 => assert now = 4 ns and i = 3 severity failure;
      when others => assert false severity failure;
    end case;
    report "listed run " & integer'image(n);
  end process;

  check : process is
  begin
    wait for 10 ns;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
