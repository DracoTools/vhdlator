-- DESCRIPTION: Verilator: VHDL asynchronous reset branch that reads a signal the process waits on
--
-- "if reset = '1' then out1 <= state; elsif clk'event and clk = '1'" in a
-- "process (all)": while the reset holds, every change of state runs the
-- process again. Lowered as an edge-triggered block it ran only on the
-- edges of clk and reset (GHDL synth issue2850). With "process (clk, reset)"
-- it does not, and that one stays an edge-triggered block.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_asyncresetall is
end entity;

architecture a of t_vhdl_asyncresetall is
  signal clk, reset, state, in1, q_all, q_edge : std_logic := '0';
begin
  process (all)
  begin
    if reset = '1' then
      q_all <= state;
    elsif clk'event and clk = '1' then
      q_all <= in1;
    end if;
  end process;

  process (clk, reset)
  begin
    if reset = '1' then
      q_edge <= state;
    elsif rising_edge(clk) then
      q_edge <= in1;
    end if;
  end process;

  process
  begin
    in1 <= '1';
    clk <= '1';
    wait for 1 ns;
    assert q_all = '1' and q_edge = '1' severity failure;
    reset <= '1';
    wait for 1 ns;
    assert q_all = '0' and q_edge = '0' severity failure;
    state <= '1';
    wait for 1 ns;
    assert q_all = '1' severity failure;
    assert q_edge = '0' severity failure;
    report "asyncresetall done";
    std.env.finish;
  end process;
end architecture;
