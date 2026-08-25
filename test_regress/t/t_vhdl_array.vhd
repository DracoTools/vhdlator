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

  constant grid : pair_list := ((1, 2), (3, 4), (5, 6));

  -- An index constraint written as a type mark stands for that type's range
  type small is range 0 to 3;
  type bits is array (small range <>) of bit;
  subtype nibble is bits(small);

  constant n : nibble := "1011";

  -- An aggregate whose elements are slices of the same type spans as many
  -- indices as they are long
  constant j : bits(0 to 3) := (n(0 to 1), n(2 to 3));

begin

  process
  begin
    assert grid(1)(1) = 1 report "grid(1)(1)" severity failure;
    assert grid(2)(2) = 4 report "grid(2)(2)" severity failure;
    assert grid(3)(1) = 5 report "grid(3)(1)" severity failure;
    assert n'length = 4 report "n'length" severity failure;
    assert n(0) = '1' and n(1) = '0' and n(3) = '1' report "n" severity failure;
    assert j = n report "j" severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
