-- DESCRIPTION: Verilator: VHDL projected waveform, parts at indices only running decides
--
-- An element or a slice at run-time indices preempts and rejects its own pending transactions, and its part of a whole-signal one.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_wavedyn is end entity;
architecture a of t_vhdl_wavedyn is
  signal s : std_logic_vector(7 downto 0) := x"00";
begin
  process
    variable i : natural := 2;
  begin
    -- Inertial on a run-time index: the second preempts the first, s(2) only
    s(i) <= '1' after 10 ns;
    s(i) <= '0' after 5 ns;
    s(i + 1) <= '1' after 3 ns;
    wait for 20 ns;
    assert s = "00001000" report "1: " & to_string(s) severity failure;
    -- Transport keeps both
    i := 5;
    s(i) <= transport '1' after 10 ns;
    s(i) <= transport '0' after 15 ns;
    wait for 12 ns;
    assert s(5) = '1' report "2" severity failure;
    wait for 5 ns;
    assert s(5) = '0' report "3" severity failure;
    -- A run-time element preempts its part of a whole transaction
    s <= x"ff" after 10 ns;
    i := 0;
    s(i) <= '0' after 5 ns;
    wait for 12 ns;
    assert s = "11111110" report "4: " & to_string(s) severity failure;
    -- A slice at run-time ends
    i := 4;
    s(i + 3 downto i) <= "0101" after 2 ns;
    wait for 3 ns;
    assert s = "01011110" report "5: " & to_string(s) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
