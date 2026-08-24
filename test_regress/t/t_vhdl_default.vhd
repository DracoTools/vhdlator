-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An object with no initial value starts at the leftmost value of its
-- subtype (6.4.2.2), which for INTEGER is not zero.

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

begin

  main : process is
    variable i  : integer;
    variable n  : natural;
    variable p  : positive;
    variable r  : real;
    variable c  : colour;
    variable w  : warm;
    variable pt : point;
    variable sz : sized;
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

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
