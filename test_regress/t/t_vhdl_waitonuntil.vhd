-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- "wait on S until C" looks at C after each event on S, and only then (10.2):
-- a change of a signal C reads, outside S, does not wake the process. OSVVM's
-- UART receiver samples a bit when the 16x clock rises while SampleBit is set;
-- SampleBit rises two delta cycles after a clock edge and falls two delta
-- cycles after the next, so each pulse holds one clock edge and each bit is
-- taken once.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_waitonuntil is
end entity;

architecture a of t_vhdl_waitonuntil is
  signal clk : std_logic := '0';
  signal cnt : unsigned(3 downto 0) := "0110";
  signal sb : std_logic := '0';
  signal n : integer := 0;
  signal m : integer := 0;
  constant baud : time := 8 us;
begin
  clkproc : process
  begin
    wait for baud / 16;
    clk <= '0', '1' after baud / 32;
  end process;

  cntproc : process
  begin
    wait until clk = '1';
    cnt <= cnt - 1;
  end process;

  sb <= '1' when cnt = 0 else '0';

  -- Woken by clk alone
  handler : process
  begin
    wait on clk until clk = '1' and sb = '1';
    n <= n + 1;
  end process;

  -- Woken by clk and sb, which the condition reads: twice per pulse
  both : process
  begin
    wait until clk = '1' and sb = '1';
    m <= m + 1;
  end process;

  process
  begin
    wait for 40 us;
    report "n=" & integer'image(n) & " m=" & integer'image(m);
    assert n = 5 severity failure;
    assert m = 10 severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
