-- DESCRIPTION: Verilator: VHDL access to an array of two open dimensions, allocated in one shape
--
-- Every allocator of the access type makes an object of one shape that the
-- analysis settles, so the object is held as that shape: allocated with an
-- index constraint and from a constant of the shape, indexed, assigned,
-- compared (bugs.md 388aw; VESTS tc521).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_acc2dshape is
end entity;

architecture test of t_vhdl_acc2dshape is
  type grid is array (natural range <>, natural range <>) of bit;
  subtype grid23 is grid(0 to 1, 0 to 2);
  type grid_ptr is access grid;
  constant c : grid23 := ("101", "010");
begin
  process
    variable a, b : grid_ptr;
  begin
    a := new grid(0 to 1, 0 to 2);
    assert a.all = grid23'(others => (others => '0')) severity failure;
    a(0, 1) := '1';
    assert a(0, 1) = '1' and a(1, 2) = '0' severity failure;
    b := new grid'(c);
    assert b.all = c severity failure;
    b.all := a.all;
    assert b(0, 1) = '1' severity failure;
    assert a'length(1) = 2 and a'length(2) = 3 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
