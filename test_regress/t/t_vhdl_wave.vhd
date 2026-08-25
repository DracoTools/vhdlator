-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A waveform of more than one element schedules each at its own time
-- (10.5.2.2, 11.6), and a port left open takes its own default (6.5.6.3).

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_wave_sub is
  port (a : in  std_logic := '1';
        b : in  std_logic := '0';
        y : out std_logic);
end entity;

architecture rtl of t_vhdl_wave_sub is
begin
  y <= a xor b;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use std.env.all;

entity t_vhdl_wave is
end entity;

architecture tb of t_vhdl_wave is
  signal s : std_logic := '0';
  signal t : std_logic := '0';
  signal y1, y2 : std_logic;
  signal d : std_logic := '1';
begin

  -- Concurrent, three elements
  s <= '1' after 10 ns, '0' after 20 ns, '1' after 30 ns;

  -- One port left out, one written "open"
  u1 : entity work.t_vhdl_wave_sub port map (a => d, y => y1);
  u2 : entity work.t_vhdl_wave_sub port map (a => open, b => d, y => y2);

  process
  begin
    -- Sequential, two elements
    t <= '1' after 5 ns, '0' after 15 ns;

    wait for 1 ns;
    assert y1 = '1' report "y1" severity failure;
    assert y2 = '0' report "y2" severity failure;

    wait for 11 ns;   -- 12 ns
    assert s = '1' report "s at 12" severity failure;
    assert t = '1' report "t at 12" severity failure;

    wait for 10 ns;   -- 22 ns
    assert s = '0' report "s at 22" severity failure;
    assert t = '0' report "t at 22" severity failure;

    wait for 10 ns;   -- 32 ns
    assert s = '1' report "s at 32" severity failure;

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
