-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- numeric_std's "*" of two UNSIGNED or two SIGNED, emitted natively under
-- the nine values: as long as both operands together, all 'X' where either
-- holds a metavalue, 'L' and 'H' read as '0' and '1'; products of up to 128
-- elements, past 64 as neorv32's 33 by 33 bit multiplier is

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_nine_mul is
end entity;

architecture a of t_vhdl_nine_mul is
  signal ua : unsigned(2 downto 0) := "101";
  signal ub : unsigned(4 downto 0) := "11011";
  signal sa : signed(32 downto 0);
  signal sb : signed(32 downto 0);
  signal sp : signed(65 downto 0);
  signal wa, wb : unsigned(63 downto 0);
  signal wp : unsigned(127 downto 0);
  signal up : unsigned(7 downto 0);
begin
  -- Concurrent, as neorv32 writes it in a clocked process
  sp <= sa * sb;
  wp <= wa * wb;
  up <= ua * ub;

  process
  begin
    sa <= to_signed(-123456789, 33);
    sb <= to_signed(987654321, 33);
    wa <= x"FFFFFFFFFFFFFFFF";
    wb <= x"0000000100000003";
    wait for 1 ns;
    assert up = to_unsigned(5 * 27, 8) report "up=" & to_string(up) severity failure;
    assert sp = to_signed(-123456789, 33) * to_signed(987654321, 33) severity failure;
    assert to_hstring(sp) = "FFE4ECEEB0400AC7B" report "sp=" & to_hstring(sp) severity failure;
    assert to_hstring(wp) = "0000000100000002FFFFFFFEFFFFFFFD"
      report "wp=" & to_hstring(wp) severity failure;
    -- 'L' and 'H' are '0' and '1'
    ua <= "H0H";
    wait for 1 ns;
    assert up = to_unsigned(5 * 27, 8) report "up=" & to_string(up) severity failure;
    -- A metavalue makes every element 'X'
    ua <= "1X1";
    sb(7) <= 'U';
    wait for 1 ns;
    assert to_string(up) = "XXXXXXXX" report "up=" & to_string(up) severity failure;
    assert sp'length = 66 and to_string(sp) = (1 to 66 => 'X') severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
