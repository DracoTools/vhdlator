-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- STD_ULOGIC objects read and written over VPI (t_vhdl_nine_vpi.cpp)

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_nine_vpi is
end entity;

architecture a of t_vhdl_nine_vpi is
  signal never : std_logic_vector(3 downto 0);
  signal mix   : std_logic_vector(0 to 8) := "UX01ZWLH-";
  signal s     : std_logic := 'Z';
  signal inp   : std_logic_vector(3 downto 0) := "0000";
  signal res   : std_logic_vector(3 downto 0);
begin
  res <= not inp;
end architecture;
