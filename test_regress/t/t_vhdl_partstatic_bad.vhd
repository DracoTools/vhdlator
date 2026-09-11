-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A port associated in parts names each part with a locally static name
-- (6.5.7.1): not through a generic, a generate parameter, or the range of a
-- signal a generic constrains. A port that takes its bounds from its actual
-- cannot take them from a conversion to a type with none. References: nvc
-- 1.22.1, GHDL 7.0.0-dev, Questa 2025.2 (which takes the conversion).

entity sub is
  port (d : in bit_vector(7 downto 0));
end entity;

architecture a of sub is
begin
end architecture;

entity open_in is
  port (d : in bit_vector);
end entity;

architecture a of open_in is
begin
end architecture;

entity t is
  generic (g : natural := 4);
end entity;

architecture a of t is
  constant c : natural := 4;
  signal s4 : bit_vector(3 downto 0);
  signal sg : bit_vector(g - 1 downto 0);
  signal sc : bit_vector(c - 1 downto 0);
  signal x : bit_vector(7 downto 0);
begin
  u1 : entity work.sub port map (d(c - 1 downto 0) => sc, d(7 downto c) => x(7 downto c));
  u2 : entity work.sub port map (d(s4'range) => s4, d(7 downto 4) => s4);
  u3 : entity work.sub port map (d(g - 1 downto 0) => sg, d(7 downto 4) => s4);  -- Bad
  u4 : entity work.sub port map (d(sg'range) => sg, d(7 downto 4) => s4);  -- Bad
  u6 : entity work.open_in port map (d => bit_vector(x(3 downto 0)));  -- Bad
  u7 : entity work.open_in port map (d => x(3 downto 0));
  gen : for i in 0 to 0 generate
    u5 : entity work.sub port map (d(7 downto 1) => x(7 downto 1), d(i) => x(i));  -- Bad
  end generate;
end architecture;
