-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A type written with character literals is not STD_ULOGIC because it looks
-- like it. Only BIT and the nine values of STD_ULOGIC are the logic types
-- (5.2.2, and 1164); a type of three or four values of its own keeps every
-- one of them distinct, and a signal of one wakes a process that waits on
-- the value it takes.

use std.env.all;

entity t_vhdl_enumchar is
end entity;

architecture tb of t_vhdl_enumchar is
  type mvl4 is ('Z', '0', '1', 'X');           -- not STD_ULOGIC
  type word is array (0 to 3) of mvl4;
  type tri is ('0', '1', 'Z');
  signal s    : mvl4 := '0';
  signal a    : word := ('0', '0', '0', '0');
  signal t    : tri  := '0';
  signal b    : bit  := '0';
  signal seen : integer := 0;
begin

  -- A process that waits on one element of the array, by a fixed index
  watch: process is
  begin
    wait until a(3) = 'X';
    seen <= seen + 1;
    wait until a(3) = 'Z';
    seen <= seen + 1;
    wait;
  end process;

  main: process is
    variable i : integer;
  begin
    -- Each value is its own, and none of them collapses onto another
    assert mvl4'pos('Z') = 0 report "'Z' is not first" severity failure;
    assert mvl4'pos('X') = 3 report "'X' is not last" severity failure;
    assert s = '0' report "s did not start at '0'" severity failure;

    s <= 'X' after 10 ns;
    a(3) <= 'X' after 10 ns;
    t <= 'Z' after 10 ns;
    b <= '1' after 10 ns;
    wait for 20 ns;
    assert s = 'X' report "s lost 'X'" severity failure;
    assert a(3) = 'X' report "a(3) lost 'X'" severity failure;
    assert a(0) = '0' report "a(0) moved" severity failure;
    assert t = 'Z' report "t lost 'Z'" severity failure;
    assert b = '1' report "b lost '1'" severity failure;
    assert seen = 1 report "the watcher did not wake on a(3)" severity failure;

    -- ... and the same through an index only running decides
    i := 3;
    a(i) <= 'Z' after 10 ns;
    wait for 20 ns;
    assert a(i) = 'Z' report "a(i) lost 'Z'" severity failure;
    assert seen = 2 report "the watcher did not wake a second time" severity failure;

    -- BIT keeps being one bit, so what looks like logic still is
    assert bit'pos('1') = 1 report "BIT moved" severity failure;
    stop(0);
  end process;

end architecture;
