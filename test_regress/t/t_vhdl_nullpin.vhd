-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A null array given to open ports, in and out: its range folds where it
-- is declared and the port's comes with the instance, and the two were
-- held with different widths, which the build refused as WIDTHEXPAND.
-- A null array is one place everywhere (5.3.2.1), and holds nothing.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
entity t_vhdl_nullpin_sub is
  port (ui : in unsigned; uo : out unsigned);
end entity;
architecture a of t_vhdl_nullpin_sub is
begin
  uo <= ui;
  process
  begin
    wait for 1 ns;
    assert ui'length = 0 and uo'length = 0 severity failure;
    assert ui'left = -1 and ui'right = 0 severity failure;
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
entity t_vhdl_nullpin is
end entity;
architecture a of t_vhdl_nullpin is
  signal inull : unsigned(-1 downto 0);
  signal onull : unsigned(-1 downto 0);
begin
  u : entity work.t_vhdl_nullpin_sub port map (ui => inull, uo => onull);
  process
  begin
    wait for 2 ns;
    assert onull'length = 0 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
