-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A part of a formal may be named as deep as its type goes: an element of a
-- two-dimensional array takes an index per dimension, "p(1,1)", and an
-- element of an array of arrays takes one index per level, "p(1)(2)"
-- (6.5.7.1, 8.4). Both name one element, and a name with fewer indices than
-- the array has dimensions names nothing at all.

entity t_vhdl_elemdeep is
end entity;

architecture rtl of t_vhdl_elemdeep is
  type    mat_t is array (integer range <>, integer range <>) of integer;
  subtype m22_t is mat_t(1 to 2, 1 to 2);
  type    row_t is array (integer range <>) of integer;
  subtype r2_t  is row_t(1 to 2);
  type    grid_t is array (integer range <>) of r2_t;
  subtype g22_t is grid_t(1 to 2);
  type    rec_t is record
    a : r2_t;
  end record;
begin
  main : process
    -- Swaps the two dimensions of its inout, so a wrong part would show
    procedure Flip(P : inout m22_t) is
      variable was : m22_t := P;
    begin
      for i in 1 to 2 loop
        for j in 1 to 2 loop
          P(i, j) := was(j, i);
        end loop;
      end loop;
    end procedure;

    procedure Bump(G : inout g22_t; step : in integer) is
    begin
      for i in 1 to 2 loop
        for j in 1 to 2 loop
          G(i)(j) := G(i)(j) + step;
        end loop;
      end loop;
    end procedure;

    -- A part below a record element, "R.a(2)"
    procedure Widen(R : inout rec_t) is
    begin
      R.a(1) := R.a(1) * 10;
      R.a(2) := R.a(2) * 10;
    end procedure;

    variable a, b, c, d : integer;
    variable g : g22_t;
    variable r : rec_t;
  begin
    -- Each element of the formal named on its own, by two indices
    a := 1; b := 2; c := 3; d := 4;
    Flip(P(1, 1) => a, P(1, 2) => b, P(2, 1) => c, P(2, 2) => d);
    assert a = 1 report "bad a" severity error;
    assert b = 3 report "bad b" severity error;
    assert c = 2 report "bad c" severity error;
    assert d = 4 report "bad d" severity error;

    -- The same for an array of arrays, an index per level
    a := 10; b := 20; c := 30; d := 40;
    Bump(G(1)(1) => a, G(1)(2) => b, G(2)(1) => c, G(2)(2) => d, step => 5);
    assert a = 15 report "bad g11" severity error;
    assert b = 25 report "bad g12" severity error;
    assert c = 35 report "bad g21" severity error;
    assert d = 45 report "bad g22" severity error;

    -- A part below a record element
    a := 7; b := 8;
    Widen(R.a(1) => a, R.a(2) => b);
    assert a = 70 report "bad r.a(1)" severity error;
    assert b = 80 report "bad r.a(2)" severity error;

    -- The whole formal, written the ordinary way, still works
    g := ((1, 2), (3, 4));
    Bump(g, 1);
    assert g(2)(2) = 5 report "bad whole" severity error;

    report "*-* All Finished *-*";
    std.env.stop;
    wait;
  end process;
end architecture;
