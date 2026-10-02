-- DESCRIPTION: Verilator: VHDL std.env.stop ends the run where it is called
--
-- STOP and FINISH end the run at the call (16.5): the subprograms that led
-- to it return at once and the process that called them goes no further.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_stopnow is
end entity;

architecture test of t_vhdl_stopnow is
  signal other_ran : boolean := false;
  procedure inner (n : integer) is
  begin
    report "inner before";
    if n > 0 then
      std.env.stop;
      report "inner after stop" severity failure;
    end if;
  end procedure;
  procedure outer (n : integer) is
  begin
    inner(n);
    report "outer after inner" severity failure;
  end procedure;
  impure function f (n : integer) return integer is
  begin
    outer(n);
    report "function after outer" severity failure;
    return 5;
  end function;
begin
  main : process
    variable x : integer;
  begin
    wait for 1 ns;
    report "before";
    x := f(1);
    report "after function" severity failure;
    std.env.finish;
    wait;
  end process;
  other : process
  begin
    wait for 1 ns;
    wait for 0 ns;
    wait for 0 ns;
    report "the other process still ran in that cycle";
    wait;
  end process;
end architecture;
