-- DESCRIPTION: Verilator Example: VPI against a VHDL design
--
-- This file ONLY is placed under the Creative Commons Public Domain
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity counter is
  generic (WIDTH : integer := 8);
  port (clk : in  std_logic;
        rst : in  std_logic);
end entity;

architecture rtl of counter is
  signal count : std_logic_vector(WIDTH - 1 downto 0) := (others => '0');
  signal flag  : std_logic := '0';
begin
  process (clk)
  begin
    if rising_edge(clk) then
      if rst = '1' then
        count <= (others => '0');
      else
        count <= std_logic_vector(unsigned(count) + 1);
      end if;
      flag <= not flag;
    end if;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity top is
  port (clk : in std_logic;
        rst : in std_logic);
end entity;

architecture tb of top is
begin
  u_cnt : entity work.counter
    generic map (WIDTH => 8)
    port map (clk => clk, rst => rst);
end architecture;
