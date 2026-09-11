-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A port associated in parts is associated in every scalar subelement, and
-- an element of it associated in parts of its own is so in every bit
-- (6.5.7.1). References: GHDL 7.0.0-dev, Questa 2025.2 (nvc 1.22.1 takes it).

package pkg is
  type my_arr is array (1 to 2) of bit_vector(7 downto 0);
end package;

use work.pkg.all;

entity sub is
  port (v : out my_arr);
end entity;

architecture a of sub is
begin
  v <= (others => (others => '0'));
end architecture;

use work.pkg.all;

entity t is
end entity;

architecture a of t is
  signal s1, s2 : bit_vector(7 downto 0);
begin
  u1 : entity work.sub port map (v(1) => s1, v(2)(7 downto 4) => s2(7 downto 4), v(2)(3 downto 0) => s2(3 downto 0));
  u2 : entity work.sub port map (v(1) => s1, v(2)(7 downto 6) => s2(7 downto 6));  -- Bad
end architecture;
