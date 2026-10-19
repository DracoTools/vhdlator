-- DESCRIPTION: Verilator Example: VHDL tracing, the traced unit
--
-- This file ONLY is placed under the Creative Commons Public Domain
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity counter is
  generic (WIDTH : positive := 8);
  port (
    clk   : in  std_logic;
    rst   : in  std_logic;
    en    : in  std_logic;
    count : out unsigned(WIDTH - 1 downto 0);
    wrap  : out std_logic
  );
end entity;

architecture rtl of counter is
  signal value : unsigned(WIDTH - 1 downto 0);
begin
  process (clk)
  begin
    if rising_edge(clk) then
      if rst = '1' then
        value <= (others => '0');
      elsif en = '1' then
        value <= value + 1;
      end if;
    end if;
  end process;

  count <= value;
  wrap  <= '1' when value = (value'range => '1') else '0';
end architecture;
