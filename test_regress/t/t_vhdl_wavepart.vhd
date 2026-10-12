-- DESCRIPTION: Verilator: VHDL projected waveform of an element or a slice
--
-- A driver drives each scalar subelement on its own (14.7.2): an inertial
-- assignment to one element preempts that element's pending transactions
-- and no other's, transport keeps them, pulse rejection works per element,
-- an element preempts its part of a pending whole-signal transaction, and
-- two parts given in one delta both arrive.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_wavepart is end entity;
architecture a of t_vhdl_wavepart is
  signal s : std_logic_vector(3 downto 0) := "0000";
  signal u : std_logic_vector(0 to 3) := "0000";
  signal a, t : std_logic := '0';
begin
  -- A concurrent assignment to one element, inertial
  t <= a after 4 ns;

  process
  begin
    -- Inertial: the second preempts the first on element 0 alone
    s(0) <= '1' after 10 ns;
    s(0) <= '0' after 5 ns;
    s(3 downto 2) <= "11" after 3 ns;
    wait for 20 ns;
    assert s = "1100" report "1: " & to_string(s) severity failure;
    -- Transport on element 1 keeps the earlier transaction
    s(1) <= transport '1' after 10 ns;
    s(1) <= transport '0' after 15 ns;
    wait for 12 ns;
    assert s(1) = '1' report "2" severity failure;
    wait for 5 ns;
    assert s(1) = '0' report "3" severity failure;
    -- Pulse rejection on element 2: the '0' due within the window is gone
    s(2) <= '0' after 4 ns;
    s(2) <= '1' after 6 ns;
    wait for 5 ns;
    assert s(2) = '1' report "4" severity failure;
    -- A part preempts the elements it drives of a whole transaction
    s <= "0000" after 10 ns;
    s(0) <= '1' after 5 ns;
    wait for 12 ns;
    assert s = "0001" report "5: " & to_string(s) severity failure;
    -- Two parts in one delta both arrive
    s(3) <= '1';
    s(2) <= '1';
    wait for 0 ns;
    assert s = "1101" report "6: " & to_string(s) severity failure;
    -- An ascending vector, a slice
    u(1 to 2) <= "11" after 2 ns;
    u(0) <= '1' after 1 ns;
    wait for 3 ns;
    assert u = "1110" report "7: " & to_string(u) severity failure;
    -- The concurrent element: a pulse shorter than its delay is rejected
    a <= '1';
    wait for 2 ns;
    a <= '0';
    wait for 6 ns;
    assert t = '0' report "8" severity failure;
    a <= '1';
    wait for 6 ns;
    assert t = '1' report "9" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
