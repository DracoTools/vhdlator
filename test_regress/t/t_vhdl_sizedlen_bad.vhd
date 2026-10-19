-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- TO_UNSIGNED of a size other than the target's length, in a concurrent assignment: an
-- error when it runs (10.5.2.1), and the value of ten million bits is never built (GHDL gna
-- issue692).

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_sizedlen_bad is
end entity;

architecture a of t_vhdl_sizedlen_bad is
  signal ok : unsigned(15 downto 0);
  signal bad : unsigned(15 downto 0);
begin
  ok <= to_unsigned(5, 16);
  bad <= to_unsigned(integer(0.0), 10 ** 7);
  process is
  begin
    wait for 1 ns;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
