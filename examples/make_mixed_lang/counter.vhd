-- DESCRIPTION: Verilator Example: the VHDL half of a mixed-language design
--
-- This file ONLY is placed under the Creative Commons Public Domain
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A VHDL entity a Verilog testbench instantiates, whose architecture in turn
-- instantiates a Verilog module. Both directions in one design.

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

  -- A Verilog module is seen from VHDL as a component. VHDL is not case
  -- sensitive, so the names may be spelled either way here.
  component adder is
    generic (width : integer);
    port (a : in  std_logic_vector(7 downto 0);
          b : in  std_logic_vector(7 downto 0);
          s : out std_logic_vector(7 downto 0));
  end component;

  signal count : std_logic_vector(7 downto 0) := (others => '0');
  signal next_count : std_logic_vector(7 downto 0);

begin

  u_add : component adder
    generic map (width => 8)
    port map (a => count, b => x"01", s => next_count);

  process (clk)
  begin
    if rising_edge(clk) then
      if rst = '1' then
        count <= (others => '0');
      else
        count <= next_count;
      end if;
    end if;
  end process;

  q <= count;

end architecture;
