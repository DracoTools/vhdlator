-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Access types: values that point at objects made by an allocator and read
-- through ".all", including a type that points at itself (5.4).

use std.env.all;

entity t_vhdl_access is
end entity;

architecture tb of t_vhdl_access is

  type int_ptr is access integer;

  type rec is record
    a : integer;
    b : integer;
  end record;
  type rec_ptr is access rec;

  -- An incomplete declaration, completed after the access type names it
  type item;
  type item_ptr is access item;
  type item is record
    value : integer;
    next_item : item_ptr;
  end record;

begin

  process
    variable p, q : int_ptr;
    variable r : rec_ptr;
    variable head, walk : item_ptr;
    variable sum : integer := 0;
  begin
    -- An access value starts out pointing at nothing
    assert p = null report "p starts null" severity failure;

    p := new integer'(42);
    assert p /= null report "p not null" severity failure;
    assert p.all = 42 report "p.all" severity failure;
    p.all := 7;
    assert p.all = 7 report "p.all after write" severity failure;

    -- With no value, the designated subtype's own default
    q := new integer;
    assert q.all = integer'left report "q default" severity failure;

    deallocate(p);
    assert p = null report "p after deallocate" severity failure;

    -- An element of what an access value points at needs no ".all"
    r := new rec'(1, 2);
    assert r.a = 1 report "r.a" severity failure;
    assert r.all.b = 2 report "r.all.b" severity failure;
    r.a := 5;
    assert r.a = 5 report "r.a after write" severity failure;

    -- A list of objects that point at one another
    for i in 1 to 4 loop
      walk := new item;
      walk.value := i;
      walk.next_item := head;
      head := walk;
    end loop;
    walk := head;
    while walk /= null loop
      sum := sum + walk.value;
      walk := walk.next_item;
    end loop;
    assert sum = 10 report "sum " & integer'image(sum) severity failure;

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
