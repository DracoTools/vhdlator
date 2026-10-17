-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An aggregate of a two-dimensional array whose elements are checked against
-- their subtype: its values are rows, not elements, and are not checked as
-- elements (NVC default1, which the check of real7 broke)

entity t_vhdl_agg2d_elem is
end entity;

architecture test of t_vhdl_agg2d_elem is
begin
  process is
    type mat2x2 is array (1 to 2, 1 to 2) of natural;
    type rmat is array (1 to 2, 1 to 2) of real;
    variable m : mat2x2;
    variable r : rmat := ((1.0, 2.0), (3.0, 4.0));
  begin
    assert m = ((0, 0), (0, 0)) severity failure;
    m := ((1, 2), (3, 4));
    assert m(2, 1) = 3 severity failure;
    assert r(2, 2) = 4.0 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
