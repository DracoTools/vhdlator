-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- "s'last_value(k)": 'LAST_VALUE and 'DRIVING_VALUE take no parameter and
-- are of the prefix's own type, so an expression in parentheses after them
-- indexes that value (16.2.4, 8.4). It was read as the attribute's
-- parameter, and the whole array was given where an element was wanted.
-- nvc 1.22.1 and Questa 25.1std accept this; GHDL 7.0.0-dev stops on an
-- internal error.
library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_lastvalidx is
end entity;

architecture sim of t_vhdl_lastvalidx is
  signal r : std_logic_vector(0 to 7) := (others => '0');
  signal b : bit_vector(3 downto 0) := "0000";
  signal w : bit_vector(0 to 20) := (others => '0');
  signal log : integer := 0;

  -- Through a signal parameter bound to a slice
  procedure watch(signal s : in bit_vector; signal n : inout integer) is
  begin
    wait until s'event;
    if s(s'left + 1) = '1' and s'last_value(s'left + 1) = '0' then
      n <= n + 1;
    end if;
  end;
begin
  process
    variable x : std_logic;
    variable y : bit;
  begin
    wait for 1 ns;
    r <= "00100001";
    b(1) <= '1';
    wait for 1 ns;
    x := r'last_value(2);
    y := b'last_value(1);
    assert x = '0' and y = '0' and r(2) = '1' and b(1) = '1' severity failure;
    assert r'last_value(7) = '0' and r'driving_value(7) = '1' severity failure;
    assert b'last_value(3 downto 2) = "00" severity failure;
    r <= "11000000";
    wait for 1 ns;
    assert r'last_value(2) = '1' and r'last_value(0) = '0' severity failure;
    assert r'driving_value(1) = '1' and r'driving_value(2) = '0' severity failure;
    wait for 5 ns;
    assert log = 1 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;

  process
  begin
    watch(w(15 to 19), log);
    wait;
  end process;

  process
  begin
    wait for 2 ns;
    w(16) <= '1';
    wait;
  end process;
end architecture;
