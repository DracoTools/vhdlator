-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A VHDL design whose objects are reached over VPI. Nothing here is written
-- for VPI's benefit: these are ordinary signals, ports and a generic.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity counter is
  generic (WIDTH : integer := 8);
  port (clk : in  std_logic;
        rst : in  std_logic;
        q   : out std_logic_vector(7 downto 0));
end entity;

architecture rtl of counter is
  signal count : unsigned(7 downto 0) := (others => '0');
  signal flag  : std_logic := '0';
begin
  process (clk, rst) is
  begin
    if rst = '1' then
      count <= (others => '0');
      flag  <= '0';
    elsif rising_edge(clk) then
      count <= count + 1;
      flag  <= not flag;
    end if;
  end process;
  q <= std_logic_vector(count);
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_vpi is
  port (clk : in  std_logic;
        rst : in  std_logic;
        q   : out std_logic_vector(7 downto 0));
end entity;

architecture tb of t_vhdl_vpi is
  component counter is
    generic (WIDTH : integer);
    port (clk : in  std_logic;
          rst : in  std_logic;
          q   : out std_logic_vector(7 downto 0));
  end component;
begin
  u_cnt : component counter
    generic map (WIDTH => 8)
    port map (clk => clk, rst => rst, q => q);
end architecture;
