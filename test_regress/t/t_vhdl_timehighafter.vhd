-- DESCRIPTION: Verilator: VHDL signal transactions due after TIME'HIGH
--
-- "clk <= not clk after 100 ms" runs until the next transaction would lie
-- beyond TIME'HIGH, and then the simulation is complete (14.7.5.1).
-- Scheduled anyway, the transaction wrapped the clock round to a time
-- already gone, and the run aborted (GHDL gna issue1994). The same holds
-- for a resolved signal's driver and for a process's own assignment.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_timehighafter is
end entity;

architecture sim of t_vhdl_timehighafter is
  signal clk : std_logic := '1';
  signal b : bit := '0';
  signal r : std_logic := '0';
begin
  clk <= not clk after 100 ms;
  r <= not r after 100 ms;
  r <= 'Z';

  process is
  begin
    b <= not b after 100 ms;
    wait on b;
  end process;

  process is
  begin
    wait until rising_edge(clk);
  end process;

  process is
  begin
    wait for 9223 sec;
    report "timehighafter reached 9223 sec";
    wait;
  end process;
end architecture;
