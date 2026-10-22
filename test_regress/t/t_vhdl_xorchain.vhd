-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A line of forty "xor"s of elements of two parameters over the nine values,
-- each operand held for the next. Held as a nesting, each level doubled what
-- the back end looked at, and the build did not finish. Shaped after surf's
-- CrcPkg, crc32Parallel8Byte.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_xorchain is
end entity;

architecture rtl of t_vhdl_xorchain is
  function chain (d : std_logic_vector(7 downto 0); c : std_logic_vector(39 downto 0))
    return std_logic_vector is
    variable r : std_logic_vector(1 downto 0) := (others => '0');
  begin
    r(0) := d(0) xor c(1) xor c(2) xor c(3) xor c(4) xor c(5) xor c(6) xor c(7) xor c(8) xor c(9) xor c(10) xor c(11) xor c(12) xor c(13) xor c(14) xor c(15) xor c(16) xor c(17) xor c(18) xor c(19) xor c(20) xor c(21) xor c(22) xor c(23) xor c(24) xor c(25) xor c(26) xor c(27) xor c(28) xor c(29) xor c(30) xor c(31) xor c(32) xor c(33) xor c(34) xor c(35) xor c(36) xor c(37) xor c(38) xor c(39);
    r(1) := not r(0);
    return r;
  end function;
  signal d : std_logic_vector(7 downto 0) := x"01";
  signal c : std_logic_vector(39 downto 0) := (others => '0');
  signal o : std_logic_vector(1 downto 0);
begin
  o <= chain(d, c);
  process
  begin
    wait for 1 ns;
    assert o = "01" report "one" severity failure;
    c(5) <= '1';
    wait for 1 ns;
    assert o = "10" report "two" severity failure;
    c(39) <= 'X';
    wait for 1 ns;
    assert o = "XX" report "metavalue " & to_string(o) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
