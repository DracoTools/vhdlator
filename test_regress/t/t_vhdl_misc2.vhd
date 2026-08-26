-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Four small rules that each have exactly one right answer: "mod" and "rem"
-- over a physical type (9.2.4); an operator symbol written as a name, which
-- names a reserved word and so is not case sensitive (15.4.1); an alias of an
-- alias, which denotes what the chain ends at (6.6); and a real literal that
-- no context has converted, which is still a real (9.2.7).

use std.env.all;

entity t_vhdl_misc2 is
end entity;

architecture tb of t_vhdl_misc2 is

  signal clock : bit := '0';
  alias aclock is clock;
  alias aaclock is aclock;

  attribute period : time;
  attribute period of clock : signal is 20 ns;

begin

  main : process is
    variable t : time;
    variable n : integer;
    variable r : real;
  begin
    -- A physical type has "mod" and "rem" of its own
    t := 17 ns mod 5 ns;
    assert t = 2 ns report "mod: " & time'image(t) severity failure;
    t := 17 ns rem 5 ns;
    assert t = 2 ns report "rem: " & time'image(t) severity failure;
    t := (-17 ns) mod 5 ns;
    assert t = 3 ns report "mod neg: " & time'image(t) severity failure;
    t := (-17 ns) rem 5 ns;
    assert t = -2 ns report "rem neg: " & time'image(t) severity failure;

    -- An operator symbol used as a name, in either case
    n := "mod"(17, 5);
    assert n = 2 report "mod by name" severity failure;
    n := "MOD"(17, 5);
    assert n = 2 report "MOD by name" severity failure;
    n := "+"(2, 3);
    assert n = 5 report "plus by name" severity failure;

    -- An alias of an alias denotes what the chain ends at. The attribute
    -- specified on the object is reachable through it in nvc 1.22.1 but not
    -- in GHDL 7.0.0-dev, so only what both agree on is checked here.
    assert aaclock = '0' report "alias chain value" severity failure;
    assert aclock'period = 20 ns report "alias attribute" severity failure;

    -- A real literal is a real, even where the other operand is an integer
    r := 2.0 ** 10;
    assert r > 1023.5 and r < 1024.5 report "real power" severity failure;
    r := 0.5 ** 2;
    assert r > 0.24 and r < 0.26 report "real power frac" severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
