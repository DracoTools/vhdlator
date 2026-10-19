-- DESCRIPTION: Verilator Example: VHDL tracing, the testbench
--
-- This file ONLY is placed under the Creative Commons Public Domain
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.env.all;

entity top is
end entity;

architecture tb of top is
  signal clk   : std_logic := '0';
  signal rst   : std_logic := '1';
  signal en    : std_logic := '0';
  signal count : unsigned(3 downto 0);
  signal wrap  : std_logic;
begin
  clk <= not clk after 5 ns;

  dut : entity work.counter
    generic map (WIDTH => 4)
    port map (clk => clk, rst => rst, en => en, count => count, wrap => wrap);

  stimulus : process
  begin
    wait for 22 ns;
    rst <= '0';
    en  <= '1';
    wait until rising_edge(clk) and wrap = '1';
    report "wrapped at " & time'image(now);
    wait for 30 ns;
    stop;
    wait;
  end process;
end architecture;
