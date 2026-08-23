-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- The VHDL half of a mixed-language design: a VHDL architecture that
-- instantiates a Verilog module, inside an entity a Verilog testbench
-- instantiates in turn.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_mixed is
  generic (WIDTH : integer := 8);
  port (clk : in  std_logic;
        rst : in  std_logic;
        sum : out std_logic_vector(7 downto 0));
end entity;

architecture rtl of t_vhdl_mixed is
  -- A Verilog module, seen from VHDL as a component. Note the generic is
  -- written in a different case from the Verilog parameter.
  component adder is
    generic (width : integer);
    port (a : in  std_logic_vector(7 downto 0);
          b : in  std_logic_vector(7 downto 0);
          s : out std_logic_vector(7 downto 0));
  end component;

  signal x, y, z : std_logic_vector(7 downto 0);
begin

  u_add : component adder
    generic map (width => 8)
    port map (a => x, b => y, s => z);

  process (clk, rst) is
  begin
    if rst = '1' then
      x <= (others => '0');
      y <= (others => '0');
    elsif rising_edge(clk) then
      x <= std_logic_vector(unsigned(x) + 1);
      y <= std_logic_vector(unsigned(y) + 3);
    end if;
  end process;

  sum <= z;
end architecture;
