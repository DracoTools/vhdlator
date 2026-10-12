-- DESCRIPTION: Verilator: VHDL projected waveform, a resolved signal
--
-- Each process driving a resolved signal has a projected waveform of its own: preemption and pulse rejection on one driver, with 'Z' delivered on time, leave the other driver alone.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_waveres is end entity;
architecture a of t_vhdl_waveres is
  signal s : std_logic_vector(3 downto 0) := "ZZZZ";
  signal go : boolean := false;
begin
  -- Driver A: preempts its own pending transaction
  process
  begin
    s <= "1111" after 10 ns;
    s <= "ZZ01" after 5 ns;
    wait;
  end process;
  -- Driver B: drives the upper half, and a pulse shorter than its delay
  process
  begin
    s <= "10ZZ" after 3 ns;
    wait for 6 ns;
    s <= "01ZZ" after 4 ns;   -- due at 10 ns
    wait for 1 ns;
    s <= "10ZZ" after 4 ns;   -- inertial: the "01" due at 10 ns differs, so it is rejected
    wait;
  end process;
  process
  begin
    wait for 4 ns;
    assert s = "10ZZ" report "1: " & to_string(s) severity failure;
    wait for 2 ns;
    assert s = "1001" report "2: " & to_string(s) severity failure;
    wait for 6 ns;
    -- A's "1111" was preempted, B's "01" rejected
    assert s = "1001" report "3: " & to_string(s) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
