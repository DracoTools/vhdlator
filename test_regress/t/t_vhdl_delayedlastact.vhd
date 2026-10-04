-- DESCRIPTION: Verilator: VHDL 'LAST_ACTIVE of an element's 'DELAYED
--
-- "s(0)'delayed'last_active" asks an implicit signal how long ago it was
-- last active (16.2.4) -- GHDL gna bug0140, which measures a clock period
-- this way. Nothing here marks the implicit signal's transactions, so it
-- is answered from its events, as a signal driven from outside is: GHDL
-- and Questa give 1 ns at the falling edge, as this does (nvc 1.22.1
-- gives 0 fs).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_delayedlastact is
end entity;

architecture test of t_vhdl_delayedlastact is
  signal s : std_logic_vector(31 downto 0);
  signal measured : time := -1 ns;
begin
  process
    variable period : time;
  begin
    wait until falling_edge(s(0));
    period := s(0)'delayed'last_active;
    measured <= period;
    wait;
  end process;

  process
  begin
    wait for 1 ns;
    s <= x"aa55aa55";
    wait for 1 ns;
    s <= x"55aa55aa";
    wait for 1 ns;
    assert measured = 1 ns report "measured " & time'image(measured) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
