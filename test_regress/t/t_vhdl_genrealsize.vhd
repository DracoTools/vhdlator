-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- TO_UNSIGNED of an integer worked out in reals, at a size a generic gives:
-- "to_unsigned(integer(0.25 * 2**SIZE + 0.5), SIZE)" (GHDL gna issue1820)

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_genrealsize is
  generic (SIZE : natural := 16);
end entity;

architecture a of t_vhdl_genrealsize is
  signal p : std_logic_vector(SIZE - 1 downto 0)
    := std_logic_vector(to_unsigned(integer(0.25 * 2**SIZE + 0.5), SIZE));
begin
  process
  begin
    wait for 1 ns;
    assert p = x"4001" report to_hstring(p) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
