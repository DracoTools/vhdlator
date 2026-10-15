-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Under --vhdl-metavalues, a function's constant handed to an operator
-- compiled from its body, in two copies of the function made for two calls:
-- each copy's constant has storage of its own (GHDL gna issue2626)

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_nine_constcopies is
end entity;

architecture a of t_vhdl_nine_constcopies is
  function calc_crc(arg : unsigned) return unsigned is
    constant C_POLYNOMIAL : unsigned(6 downto 0) := "0001001";
    variable crc_v : unsigned(6 downto 0);
  begin
    crc_v := (others => '0');
    for i in arg'range loop
      if to_01(arg(i)) /= crc_v(6) then
        crc_v := (crc_v(5 downto 0) & "0") xor C_POLYNOMIAL;
      else
        crc_v := (crc_v(5 downto 0) & "0");
      end if;
    end loop;
    return crc_v;
  end function;
  signal a : unsigned(39 downto 0) := x"4000000000";
  signal b : unsigned(7 downto 0) := x"41";
begin
  process
  begin
    wait for 1 ns;
    report to_string(calc_crc(a)) & " " & to_string(calc_crc(b));
    assert calc_crc(a) = "1001010" and calc_crc(b) = "1101101" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
