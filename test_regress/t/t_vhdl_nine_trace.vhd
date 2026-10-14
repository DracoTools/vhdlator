-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- STD_ULOGIC objects in a waveform (t_vhdl_nine_trace.py)

library ieee;
use ieee.std_logic_1164.all;
use std.env.all;

entity t_vhdl_nine_trace is
end entity;

architecture tb of t_vhdl_nine_trace is
  signal s    : std_logic;
  signal down : std_logic_vector(3 downto 0);
  signal up   : std_logic_vector(0 to 8) := "UX01ZWLH-";
begin
  process is
  begin
    wait for 10 ns;
    s <= '1';
    down <= "1H0L";
    up <= "111111111";
    wait for 10 ns;
    s <= 'H';
    down <= "ZWX-";
    up <= "HHHHLLLLH";
    wait for 10 ns;
    s <= 'L';
    assert down = "ZWX-" severity failure;
    wait for 10 ns;
    finish;
  end process;
end architecture;
