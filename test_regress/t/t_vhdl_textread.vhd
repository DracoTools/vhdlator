-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

use std.textio.all;

entity t_vhdl_textread is
end entity;

architecture a of t_vhdl_textread is
begin

  main : process is
    variable l : line;
    variable b : boolean;
    variable good : boolean;
    variable c : character;
    variable r : real;
    variable t : time;
    variable v : bit_vector(3 downto 0);
    variable w : bit_vector(7 downto 0);
    variable i : integer;
  begin
    -- A BOOLEAN is read as the name of its value, in either case, and the
    -- name ends where a character that cannot be in one begins
    l := new string'("  TRUE,false");
    read(l, b, good);
    assert good report "boolean not read" severity failure;
    assert b report "boolean is false" severity failure;
    assert l.all(l'left) = ',' report "boolean took the comma" severity failure;
    read(l, b, good);
    assert not good report "comma read as a boolean" severity failure;
    assert l.all(l'left) = ',' report "failed read moved the line" severity failure;
    read(l, c, good);
    read(l, b, good);
    assert good and not b report "false not read" severity failure;
    deallocate(l);

    -- A vector takes one character per bit, skipping blanks before each,
    -- and leaves what it did not read
    l := new string'("1 1 0 0 0 0 1 1 11111");
    read(l, w, good);
    assert good report "vector not read" severity failure;
    assert w = "11000011" report "vector is wrong" severity failure;
    read(l, v, good);
    assert good and v = "1111" report "second vector is wrong" severity failure;
    assert l.all(l'left) = '1' report "vector took too much" severity failure;
    deallocate(l);

    -- A number runs to the first character that cannot be part of one, and
    -- a real needs a digit after its point
    l := new string'("5.152,61.4,5.");
    read(l, r, good);
    assert good report "real not read" severity failure;
    assert r > 5.151 and r < 5.153 report "real is wrong" severity failure;
    assert l.all(l'left) = ',' report "real took the comma" severity failure;
    read(l, c, good);
    read(l, r, good);
    assert good and r > 61.39 and r < 61.41 report "second real" severity failure;
    read(l, c, good);
    read(l, r, good);
    assert not good report "5. read as a real" severity failure;
    deallocate(l);

    -- An integer stops at the point rather than reading past it
    l := new string'("42.5");
    read(l, i, good);
    assert good and i = 42 report "integer is wrong" severity failure;
    assert l.all(l'left) = '.' report "integer took the point" severity failure;
    deallocate(l);

    -- A physical value is a number and then the name of a unit
    l := new string'(" 1 ns 15 sec 1 HR x");
    read(l, t, good);
    assert good and t = 1 ns report "ns not read" severity failure;
    read(l, t, good);
    assert good and t = 15 sec report "sec not read" severity failure;
    read(l, t, good);
    assert good and t = 1 hr report "hr not read" severity failure;
    read(l, t, good);
    assert not good report "x read as a time" severity failure;
    deallocate(l);

    report "*-* All Finished *-*";
    std.env.finish;
  end process;

end architecture;
