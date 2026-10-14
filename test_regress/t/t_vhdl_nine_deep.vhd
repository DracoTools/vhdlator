-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Under --vhdl-metavalues, operators of std_logic_1164 nested deep in a
-- concurrent assignment: each operand is held once rather than written out
-- again for every lane class that reads it, which multiplied with the depth
-- (eight levels exhausted memory)

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_nine_deep is
end entity;

architecture a of t_vhdl_nine_deep is
  signal a : std_logic_vector(7 downto 0) := "1100UXZ1";
  signal b0 : std_logic_vector(7 downto 0) := "00000000";
  signal b1 : std_logic_vector(7 downto 0) := "00000001";
  signal b2 : std_logic_vector(7 downto 0) := "00000010";
  signal b3 : std_logic_vector(7 downto 0) := "00000011";
  signal b4 : std_logic_vector(7 downto 0) := "00000100";
  signal b5 : std_logic_vector(7 downto 0) := "00000101";
  signal b6 : std_logic_vector(7 downto 0) := "00000110";
  signal b7 : std_logic_vector(7 downto 0) := "00000111";
  signal b8 : std_logic_vector(7 downto 0) := "00001000";
  signal b9 : std_logic_vector(7 downto 0) := "00001001";
  signal y : std_logic_vector(7 downto 0);
begin
  y <= not ((a xor b0) and (a xor b1) and (a xor b2) and (a xor b3) and (a xor b4) and (a xor b5) and (a xor b6) and (a xor b7) and (a xor b8) and (a xor b9)) or (a and b0);
  process
  begin
    wait for 1 ns;
    report "y " & to_string(y);
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
