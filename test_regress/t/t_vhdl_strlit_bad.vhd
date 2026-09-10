-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A string or bit string literal is an array whose every character is a
-- character literal of the element type, and whose length is the number of
-- those characters: it is not stretched or cut to fit what it is given to.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_strlit_bad is
end entity;

architecture a of t_vhdl_strlit_bad is
  type arr is array (1 to 5) of bit;
  -- Legal: VHDL-2008 passes a character that is not a digit of the base
  -- through, and STD_ULOGIC has all of these
  constant ok1 : std_logic_vector(7 downto 0) := B"XXXX_01LH";
  constant ok2 : std_logic_vector(11 downto 0) := 12UX"F-";
  constant ok3 : std_logic_vector(3 downto 0) := "ZZ-W";
  constant ok4 : string := "";
  -- Illegal
  constant bad1 : bit_vector := B"0010_1020";
  constant bad2 : arr := "00_1_11";
  constant bad3 : bit_vector(1 to 8) := x"";
  signal bad4 : bit_vector(1 to 5) := "001";
begin
  process
    variable v : std_logic_vector(4 downto 0);
  begin
    v := "01";
    v := "0z1x0";
    wait;
  end process;
end architecture;
