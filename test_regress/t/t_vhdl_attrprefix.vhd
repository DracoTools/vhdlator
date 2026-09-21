-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An attribute designator takes at most one expression (8.6). What follows
-- the designator may instead be an index list or a discrete range, and then
-- the attribute name is the *prefix* of an indexed or slice name over the
-- value it denotes (8.4, 8.5) -- "s'at(2, 3)" and "s'at(2 to 4)". Parsed as
-- an argument, both are a syntax error before anything else is asked.

use std.env.all;

entity t_vhdl_attrprefix is
end entity;

architecture tb of t_vhdl_attrprefix is
  type row is array (1 to 4) of boolean;

  signal s : bit := '0';

  -- Given by a function, which is how a design usually writes an attribute
  -- value of an array type
  function makerow return row is
    variable out_v : row;
  begin
    for k in 1 to 4 loop
      out_v(k) := (k mod 2) = 0;
    end loop;
    return out_v;
  end function;

  attribute vec : row;
  attribute vec of s : signal is makerow;
begin

  main: process is
    variable v : row;
  begin
    -- One expression is the attribute's own argument, as before
    assert s'vec(2) report "vec(2) is false" severity failure;
    assert not s'vec(1) report "vec(1) is true" severity failure;

    -- A second index reads one element of a two-dimensional attribute the
    -- same way; VESTS' tc1038 covers that, and what a design writes out as
    -- a two-dimensional aggregate is a separate matter from this.

    -- A discrete range: a slice of it
    v(2 to 3) := s'vec(2 to 3);
    assert v(2) report "the slice starts false" severity failure;
    assert not v(3) report "the slice ends true" severity failure;

    report "*-* All Finished *-*" severity note;
    stop(0);
  end process;

end architecture;
