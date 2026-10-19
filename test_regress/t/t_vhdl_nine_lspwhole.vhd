-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A driver belongs to the longest static prefix of the name assigned (14.7.2).
-- "sigf(j) <= ..." with j a variable has the whole of sigf as that prefix, so
-- the process drives every element of sigf, sigf(0) included, with what it
-- started with; resolved against the concurrent "sigf(0) <= ..." that is
-- "XX" (GHDL gna bug23165). Indexed by a constant, only that element.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_nine_lspwhole is
end entity;

architecture behav of t_vhdl_nine_lspwhole is
  type sig_t is array (0 to 1) of std_logic_vector(1 downto 0);
  signal sigf : sig_t := (others => (others => '0'));
  signal sigk : sig_t := (others => (others => '0'));
  signal clk : std_logic := '0';
begin
  clk <= not clk after 5 ns;

  sigf(0) <= (others => '1');
  sigk(0) <= (others => '1');

  by_variable : process
    variable i : integer range 0 to 0 := 0;
    variable j : integer range 1 to 1 := 1;
  begin
    wait until rising_edge(clk);
    sigf(j) <= sigf(i);
  end process;

  by_constant : process
  begin
    wait until rising_edge(clk);
    sigk(1) <= sigk(0);
  end process;

  process begin
    wait for 1 ns;
    assert sigf(0) = "XX" report "sigf(0) is not XX" severity failure;
    assert sigk(0) = "11" report "sigk(0) is not 11" severity failure;
    wait for 20 ns;
    assert sigk(1) = "11" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
