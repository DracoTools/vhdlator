-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A value assigned to a signal must be as long as the signal (10.5.2.1). Where
-- generics settle both lengths it is a check when the assignment runs, here
-- at the start: y is n + 1 long and the sum n (GHDL gna bug21332).

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_genlen_bad_sub is
  generic (n : positive := 8);
  port (a : in std_logic_vector(n - 1 downto 0);
        y : out std_logic_vector(n downto 0));
end entity;

architecture behav of t_vhdl_genlen_bad_sub is
  signal temp : std_logic_vector(n - 1 downto 0);
begin
  temp <= not a;
  y <= std_logic_vector(unsigned(temp) + 1);
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_genlen_bad is
end entity;

architecture behav of t_vhdl_genlen_bad is
  signal a : std_logic_vector(7 downto 0) := x"01";
  signal y : std_logic_vector(8 downto 0);
begin
  dut : entity work.t_vhdl_genlen_bad_sub port map (a => a, y => y);
end architecture;
