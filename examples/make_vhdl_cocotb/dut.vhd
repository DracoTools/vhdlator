-- DESCRIPTION: Verilator Example: a VHDL design driven by cocotb
--
-- This file ONLY is placed under the Creative Commons Public Domain
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity dut is
  port (clk : in  std_logic;
        rst : in  std_logic;
        q   : out std_logic_vector(7 downto 0));
end entity;

architecture rtl of dut is
  signal count : unsigned(7 downto 0) := (others => '0');
begin

  process (clk, rst) is
  begin
    if rst = '1' then
      count <= (others => '0');
    elsif rising_edge(clk) then
      count <= count + 1;
    end if;
  end process;

  q <= std_logic_vector(count);

end architecture;
