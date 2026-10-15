-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A signal with a default value that a process later writes 'Z' into,
-- copied a slice at a time into an element of an array by a generate. Under
-- --no-vhdl-metavalues the 'Z' makes the back end resolve the signal as a
-- tristate net: its default became a driver of its own, or-ed into every
-- value written after (x"34" read as x"36"), and the copies, committed in
-- the order they appear rather than the order they ran, read 0 at the start.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_zinit is
end entity;

architecture a of t_vhdl_zinit is
  type res_t is array (2 downto 0) of std_ulogic_vector(31 downto 0);
  signal res : res_t;
  signal rs1 : std_ulogic_vector(31 downto 0) := x"1234A6C8";
  signal c : std_ulogic_vector(31 downto 0);
begin
  c <= rs1;
  copy_gen : for i in 0 to 3 generate
    res(1)(i*8+7 downto i*8) <= rs1(i*8+7 downto i*8);
  end generate;
  process
  begin
    wait for 1 ns;
    assert rs1 = x"1234A6C8" report "rs1 " & to_hstring(rs1) severity failure;
    assert c = x"1234A6C8" report "c " & to_hstring(c) severity failure;
    assert res(1) = x"1234A6C8" report "res " & to_hstring(res(1)) severity failure;
    rs1 <= x"0000ZZ01";
    wait for 1 ns;
    assert rs1(7 downto 0) = x"01" report "rs1 " & to_string(rs1) severity failure;
    assert res(1)(7 downto 0) = x"01" report "res " & to_string(res(1)) severity failure;
    assert rs1(31 downto 16) = x"0000" report "rs1 " & to_string(rs1) severity failure;
    rs1 <= x"00000034";
    wait for 1 ns;
    assert rs1 = x"00000034" report "rs1 " & to_hstring(rs1) severity failure;
    assert c = x"00000034" report "c " & to_hstring(c) severity failure;
    assert res(1) = x"00000034" report "res " & to_hstring(res(1)) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
