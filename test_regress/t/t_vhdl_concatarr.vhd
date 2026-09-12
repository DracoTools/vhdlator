-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- VHDL concatenates arrays of any element type (9.2.5), including ones whose
-- elements are not bits and so cannot be joined as a vector. Each operand
-- keeps its own index range, and the result is numbered from the left bound
-- of the index subtype.

entity t_vhdl_concatarr is
end entity;

use std.env.all;

architecture tb of t_vhdl_concatarr is
  type int_array is array (integer range <>) of integer;
  type real_array is array (natural range <>) of real;
begin

  process is
    variable x : int_array(1 to 3);
    variable p : int_array(6 to 6);
    variable q : int_array(6 to 7);
    variable r : real_array(0 to 2);
  begin
    -- Aggregates with index choices of their own, concatenated
    x := (6 => 7) & (6 => 2, 7 => 9);
    assert x = (7, 2, 9) report "aggregates" severity failure;

    -- The same through objects, whose own ranges say how many elements each
    p := (6 => 7);
    q := (6 => 2, 7 => 9);
    x := p & q;
    assert x = (7, 2, 9) report "objects" severity failure;

    -- An element and an array, either way round
    x := 4 & q;
    assert x = (4, 2, 9) report "element first" severity failure;
    x := q & 4;
    assert x = (2, 9, 4) report "element last" severity failure;

    -- Of a type that is not integral at all
    r := (0 => 1.5) & (0 => 2.5, 1 => 3.5);
    assert r(0) = 1.5 and r(1) = 2.5 and r(2) = 3.5
      report "reals" severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
