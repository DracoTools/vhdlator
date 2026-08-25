-- DESCRIPTION: Verilator Example: VHDL hello world
--
-- This file ONLY is placed under the Creative Commons Public Domain
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.env.all;

entity top is
end entity;

architecture rtl of top is

  signal clk   : std_logic := '0';
  signal rst   : std_logic := '1';
  signal count : unsigned(7 downto 0);

begin

  clk <= not clk after 5 ns;
  rst <= '0' after 22 ns;

  counter : process (clk)
  begin
    if rising_edge(clk) then
      if rst = '1' then
        count <= (others => '0');
      else
        count <= count + 1;
      end if;
    end if;
  end process;

  monitor : process
  begin
    wait for 100 ns;
    report "Hello World! count = " & integer'image(to_integer(count));
    assert count = 8
      report "count was " & integer'image(to_integer(count)) & ", expected 8"
      severity failure;
    stop;
    wait;
  end process;

end architecture;
