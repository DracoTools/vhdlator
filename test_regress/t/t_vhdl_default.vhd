-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An object with no initial value starts at the leftmost value of its
-- subtype (6.4.2.2), which for INTEGER is not zero.

use std.env.all;

-- An array whose length only elaboration fixes takes its default the same
-- way, from the element type
entity t_vhdl_default_sub is
  generic (n : positive);
end entity;

architecture sim of t_vhdl_default_sub is
  type sized is record
    k : integer;
    b : bit;
  end record;
  type recs is array (natural range <>) of sized;
  signal gen : recs(1 to n);
  signal genv : bit_vector(0 to n - 1);
begin
  check : process is
  begin
    assert gen(1).k = integer'left and gen(n).k = integer'left
      report "array whose length a generic gives" severity failure;
    assert gen(1).b = '0' report "record element of one" severity failure;
    assert genv = (0 to n - 1 => '0')
      report "vector whose length a generic gives" severity failure;
    wait;
  end process;
end architecture;

use std.env.all;

entity t_vhdl_default is
end entity;

architecture sim of t_vhdl_default is

  type colour is (red, green, blue);
  subtype warm is colour range red to green;

  type point is record
    x, y : integer;
  end record;

  type sized is record
    n : natural;
    c : colour;
  end record;

  type ints is array (1 to 3) of integer;
  -- A dimension of its own, and an element that packs, each need the
  -- element's default written out rather than left at zero
  type matrix is array (1 to 2, 1 to 3) of integer;
  type packedrec is record
    b : bit_vector(1 to 3);
    n : integer;
  end record;
  type recs is array (1 to 2) of packedrec;
  signal sm : matrix;
  signal sr : recs;

begin

  u_gen : entity work.t_vhdl_default_sub generic map (n => 3);

  main : process is
    variable i  : integer;
    variable n  : natural;
    variable p  : positive;
    variable r  : real;
    variable c  : colour;
    variable w  : warm;
    variable pt : point;
    variable sz : sized;
    variable ar : ints;
  begin
    assert i = integer'left report "integer" severity failure;
    assert n = 0 report "natural" severity failure;
    assert p = 1 report "positive" severity failure;
    assert r = real'left report "real" severity failure;
    assert c = red report "enumeration" severity failure;
    assert w = red report "enumeration subtype" severity failure;
    assert pt.x = integer'left and pt.y = integer'left
      report "record of integers" severity failure;
    assert sz.n = 0 and sz.c = red report "record of mixed types" severity failure;
    assert ar(1) = integer'left and ar(3) = integer'left
      report "array of integers" severity failure;
    assert sm(1, 1) = integer'left and sm(2, 3) = integer'left
      report "array of two dimensions" severity failure;
    assert sr(1).n = integer'left and sr(2).n = integer'left
      report "array of records that pack" severity failure;
    assert sr(1).b = "000" report "packed element of a record" severity failure;

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
