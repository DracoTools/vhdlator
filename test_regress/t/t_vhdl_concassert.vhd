-- DESCRIPTION: Verilator: VHDL concurrent assertion that runs when what it
-- reads changes, and at no other time
--
-- A concurrent assertion is a process that checks once at the start and
-- then waits on the signals its condition and report read (11.5). "assert
-- a = 5" reports at the start and when a changes, not when b does; one
-- that reads a string through a function reports again when the string
-- changes; and none reports twice as the design starts (GHDL gna bug7751).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_concassert is
end entity;

architecture a of t_vhdl_concassert is
  signal a, b : integer := 0;
  signal s : string(1 to 3) := "abc";
  signal w : bit_vector(63 downto 0) := (others => '0');
  function f(x : string) return integer is
  begin
    return character'pos(x(1));
  end function;
begin
  a <= 1 after 10 ns;
  b <= 1 after 20 ns, 2 after 25 ns;
  s <= "xyz" after 30 ns;
  w <= (others => '1') after 40 ns;
  assert a = 5 report "check of a" severity note;
  assert f(s) = 0 report "check of s" severity note;
  assert w(0) = w(1) and w = (63 downto 0 => '1') report "check of w" severity note;
  process is
  begin
    wait for 50 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
