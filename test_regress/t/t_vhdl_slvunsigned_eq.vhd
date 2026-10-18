-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- std_logic_unsigned's "=" and orderings compare two STD_LOGIC_VECTORs by
-- value, whatever their lengths, where the type's predefined "=" is false
-- between two lengths (GHDL gna issue377 through NVC). An explicit declaration
-- hides the implicit one of the same profile (VHDL-2008 12.4; Questa takes
-- VHDL-93 the same way). Run as VHDL-2008 and VHDL-93.

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

entity t_vhdl_slvunsigned_eq is
end entity;

architecture t of t_vhdl_slvunsigned_eq is
begin
  process
    variable v : std_logic_vector(49 downto 0) := (others => '0');
  begin
    assert v(23 downto 0) = CONV_STD_LOGIC_VECTOR(0, 23) severity failure;
    assert v(3 downto 0) = "000" severity failure;
    v(0) := '1';
    assert v(3 downto 0) = "001" severity failure;
    assert v(3 downto 0) /= "010" severity failure;
    assert v(7 downto 0) < "10" severity failure;
    assert "0000" & v(1 downto 0) >= "1" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
