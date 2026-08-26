-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Array subtypes whose element is constrained as well as their index, an
-- index constraint written as a type mark, and an aggregate whose elements
-- are themselves arrays (5.3.2.1, 9.3.3.3).

use std.env.all;

entity t_vhdl_array is
end entity;

architecture tb of t_vhdl_array is

  -- The element is unconstrained until the subtype says otherwise
  type int_grid is array (natural range <>) of integer_vector;
  subtype pair_list is int_grid(1 to 3)(1 to 2);
  type slv_grid is array (natural range <>) of bit_vector;

  constant grid : pair_list := ((1, 2), (3, 4), (5, 6));

  -- An index constraint written as a type mark stands for that type's range
  type small is range 0 to 3;
  type bits is array (small range <>) of bit;
  subtype nibble is bits(small);

  constant n : nibble := "1011";

  -- A choice may be given a value of the aggregate's own type, which then
  -- covers that whole run of indices rather than one of them (9.3.3.3)
  constant twelve : bits(0 to 1) := "10";
  constant run : bits(0 to 3) := (0 to 1 => twelve, 2 => '1', 3 => '0');
  constant srun : string(1 to 8) := (1 to 5 => "abcde", others => 'z');

  -- MINIMUM and MAXIMUM answer for a whole array of any scalar element, and
  -- for a null one they give the element type's limit at the other end
  type freqs is array (natural range <>) of time;
  constant fs : freqs(1 to 3) := (1 ns, 10 ns, 3 ns);

  -- A range attribute names a range wherever one is wanted, including as
  -- the constraint of a declaration
  function flip (x : bit_vector) return bit_vector is
    variable r : bit_vector(x'reverse_range);
  begin
    for i in x'reverse_range loop r(i) := x(i); end loop;
    return r;
  end function;

  -- An array of more than one dimension, whose bounds its own value fixes
  type int2d is array (natural range <>, natural range <>) of integer;
  constant m : int2d := ((0, 3, 4, 5), (6, 7, 8, 9));

  -- An aggregate whose elements are slices of the same type spans as many
  -- indices as they are long
  constant j : bits(0 to 3) := (n(0 to 1), n(2 to 3));

begin

  process
    variable a, b : integer;
    variable lo, hi : bits(0 to 1);
    variable bv : bit_vector(1 to 4) := "1101";
    variable nofs : freqs(3 to 1);
    variable ragged : slv_grid(0 to 2)(7 downto 0) := (others => (others => '0'));
  begin
    -- An aggregate may stand on the left of an assignment: each name takes
    -- the element of the value in its own position (10.6.2)
    (a, b) := integer_vector'(11, 22);
    assert a = 11 and b = 22 report "aggregate target" severity failure;

    -- And an element of the target may be a whole slice of the value
    (lo, hi) := n;
    assert lo = "10" report "aggregate target slice lo" severity failure;
    assert hi = "11" report "aggregate target slice hi" severity failure;

    assert grid(1)(1) = 1 report "grid(1)(1)" severity failure;
    assert grid(2)(2) = 4 report "grid(2)(2)" severity failure;
    assert grid(3)(1) = 5 report "grid(3)(1)" severity failure;
    assert n'length = 4 report "n'length" severity failure;
    assert n(0) = '1' and n(1) = '0' and n(3) = '1' report "n" severity failure;
    assert j = n report "j" severity failure;

    assert m'length(1) = 2 report "m'length(1)" severity failure;
    assert m'length(2) = 4 report "m'length(2)" severity failure;
    assert m(0, 0) = 0 report "m(0,0)" severity failure;
    assert m(0, 3) = 5 report "m(0,3)" severity failure;
    assert m(1, 2) = 8 report "m(1,2)" severity failure;

    assert flip(bv) = "1011" report "reverse_range" severity failure;

    assert run = "1010" report "run of elements in an aggregate" severity failure;
    assert srun = "abcdezzz" report "run of characters" severity failure;

    assert minimum(fs) = 1 ns report "minimum over an array" severity failure;
    assert maximum(fs) = 10 ns report "maximum over an array" severity failure;
    assert minimum(nofs) = time'high report "minimum over a null array" severity failure;
    assert maximum(nofs) = time'low report "maximum over a null array" severity failure;
    -- A subtype may constrain the element as well as the index, so an index
    -- into it gives the constrained element (5.3.3)
    assert ragged(1)'length = 8 report "element constraint length" severity failure;
    ragged(1) := "10101010";
    assert ragged(1) = "10101010" report "element constraint value" severity failure;
    assert ragged(0) = "00000000" report "element constraint default" severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
