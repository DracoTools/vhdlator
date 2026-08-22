-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t is
  port (clk : in  std_logic;
        q   : out std_logic_vector(7 downto 0));
end entity;

architecture rtl of t is
  signal count : std_logic_vector(7 downto 0) := (others => '0');
begin
  process (clk)
  begin
    if rising_edge(clk) then
      count <= std_logic_vector(unsigned(count) + 1);
    end if;
  end process;
  q <= count;
end architecture;
