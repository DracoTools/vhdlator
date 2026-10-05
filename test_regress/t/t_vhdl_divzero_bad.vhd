-- DESCRIPTION: Verilator: VHDL integer division by a zero the analyzer can
-- see, in a statement that runs
--
-- "i := 1 mod 0" is an error when it is evaluated (9.2.7): the design
-- builds, with a warning, and the run stops where the division is reached
-- -- after a branch holding another such division that is never taken
-- (nvc simp cfold, from Billowitch tc2256).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_divzero_bad is
end entity;

architecture a of t_vhdl_divzero_bad is
  signal never : boolean := false;
begin
  process is
    variable i : integer;
  begin
    if never then
      i := 7 / 0;
    end if;
    wait for 1 ns;
    report "before the division";
    i := 1 mod 0;
    report "not reached";
    wait;
  end process;
end architecture;
