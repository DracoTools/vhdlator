-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A protected type is state with methods of its own, and a shared variable
-- holds one object of it (5.6). Two objects of the same type keep separate
-- state, and a method sees only the object it was called on.

package t_vhdl_protected_pkg is
  type stats is protected
    procedure add (v : integer);
    procedure reset;
    impure function count return integer;
    impure function total return integer;
    impure function largest return integer;
  end protected;
end package;

package body t_vhdl_protected_pkg is
  type stats is protected body
    variable n : integer := 0;
    variable sum : integer := 0;
    variable most : integer := integer'low;
    procedure add (v : integer) is
    begin
      n := n + 1;
      sum := sum + v;
      if v > most then most := v; end if;
    end procedure;
    procedure reset is
    begin
      n := 0;
      sum := 0;
      most := integer'low;
    end procedure;
    impure function count return integer is
    begin
      return n;
    end function;
    impure function total return integer is
    begin
      return sum;
    end function;
    impure function largest return integer is
    begin
      return most;
    end function;
  end protected body;
end package body;

use std.env.all;
use work.t_vhdl_protected_pkg.all;

entity t_vhdl_protected is
end entity;

architecture tb of t_vhdl_protected is

  -- A protected type declared where it is used, not in a package
  type counter is protected
    procedure increment (n : integer := 1);
    impure function value return integer;
  end protected;

  type counter is protected body
    variable count : integer := 0;
    procedure increment (n : integer := 1) is
    begin
      count := count + n;
    end procedure;
    impure function value return integer is
    begin
      return count;
    end function;
  end protected body;

  shared variable c : counter;
  shared variable left : stats;
  shared variable right : stats;

  -- An object of a protected type passed as a parameter is the object
  -- itself, not a copy. The loop bound is not static, which a loop's range
  -- need not be (10.10).
  procedure fill (variable s : inout stats; n : integer) is
  begin
    for i in 1 to n loop
      s.add(i);
    end loop;
  end procedure;

begin

  process
  begin
    -- A parameter with a default may be left out of the call
    c.increment;
    c.increment(4);
    c.increment(n => 5);
    assert c.value = 10 report "counter is " & integer'image(c.value) severity failure;

    -- Two objects of one type do not share their variables
    left.add(2);
    left.add(4);
    left.add(-1);
    right.add(100);
    assert left.count = 3 report "left count" severity failure;
    assert left.total = 5 report "left total" severity failure;
    assert left.largest = 4 report "left largest" severity failure;
    assert right.count = 1 report "right count" severity failure;
    assert right.total = 100 report "right total" severity failure;

    fill(right, 4);
    assert right.count = 5 report "right count after fill" severity failure;
    assert right.total = 110 report "right total after fill" severity failure;

    -- A null range runs no iterations at all
    fill(right, 0);
    assert right.count = 5 report "a null range ran" severity failure;

    left.reset;
    assert left.count = 0 report "left count after reset" severity failure;
    assert right.count = 5 report "right kept its own state" severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
