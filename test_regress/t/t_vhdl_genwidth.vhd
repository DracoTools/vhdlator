-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- One entity whose port widths come from a generic, instantiated at two
-- widths. The bounds are not known while the source is analyzed, but they
-- are fixed once the instance is elaborated, so each instance gets its own
-- fixed-width module.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_genwidth_sub is
  generic (WIDTH : positive := 8);
  port (clk  : in  std_logic;
        d    : in  std_logic_vector(WIDTH - 1 downto 0);
        q    : out std_logic_vector(WIDTH - 1 downto 0);
        par  : out std_logic;
        n    : out integer;
        ones : out std_logic_vector(WIDTH - 1 downto 0));
end entity;

architecture rtl of t_vhdl_genwidth_sub is
  signal r : std_logic_vector(WIDTH - 1 downto 0) := (others => '0');
  signal acc : unsigned(WIDTH - 1 downto 0) := (others => '0');
begin
  process (clk)
    variable p : std_logic;
  begin
    if rising_edge(clk) then
      r <= d;
      -- 'range and 'length of an object whose bounds are not static
      p := '0';
      for i in r'range loop
        p := p xor r(i);
      end loop;
      par <= p;
      -- Arithmetic between a vector of that width and an integer
      acc <= acc + 1;
    end if;
  end process;

  q <= r;
  n <= r'length;
  ones <= (others => '1');
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use std.env.all;

entity t_vhdl_genwidth is
end entity;

architecture tb of t_vhdl_genwidth is
  signal clk : std_logic := '0';
  signal d8 : std_logic_vector(7 downto 0) := "10110001";
  signal q8, o8 : std_logic_vector(7 downto 0);
  signal p8 : std_logic;
  signal n8 : integer;
  signal d3 : std_logic_vector(2 downto 0) := "101";
  signal q3, o3 : std_logic_vector(2 downto 0);
  signal p3 : std_logic;
  signal n3 : integer;
begin
  u8 : entity work.t_vhdl_genwidth_sub generic map (WIDTH => 8)
    port map (clk => clk, d => d8, q => q8, par => p8, n => n8, ones => o8);
  u3 : entity work.t_vhdl_genwidth_sub generic map (WIDTH => 3)
    port map (clk => clk, d => d3, q => q3, par => p3, n => n3, ones => o3);

  clk <= not clk after 5 ns;

  process
  begin
    wait for 22 ns;
    assert q8 = "10110001" report "q8" severity failure;
    assert q3 = "101" report "q3" severity failure;
    assert p8 = '0' report "p8" severity failure;
    assert p3 = '0' report "p3" severity failure;
    assert n8 = 8 report "n8" severity failure;
    assert n3 = 3 report "n3" severity failure;
    assert o8 = "11111111" report "o8" severity failure;
    assert o3 = "111" report "o3" severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;
end architecture;
