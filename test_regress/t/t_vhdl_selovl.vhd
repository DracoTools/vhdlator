-- DESCRIPTION: Verilator: VHDL functions that differ only in their result,
-- told apart by the element selected from it
--
-- "fun(true).field0" calls the fun whose record has a field0 (12.5): the
-- selected name is part of the complete context that settles the call.
-- "f.a.a" does the same through two levels, and where both results have
-- the element, what the selection must give decides (GHDL gna ticket59 and
-- issue465).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_selovl_pkg is
  type rec0_t is record
    field0 : boolean;
  end record;
  type rec1_t is record
    field1 : integer;
  end record;
  function fun(val : boolean) return rec0_t;
  function fun(val : boolean) return rec1_t;
  function fun(val : boolean) return boolean;
end package;

package body t_vhdl_selovl_pkg is
  function fun(val : boolean) return rec0_t is
  begin
    return (field0 => val);
  end function;
  function fun(val : boolean) return rec1_t is
  begin
    if val then
      return (field1 => 11);
    end if;
    return (field1 => 22);
  end function;
  function fun(val : boolean) return boolean is
  begin
    return not val;
  end function;
end package body;

use work.t_vhdl_selovl_pkg.all;

entity t_vhdl_selovl is
end entity;

architecture a of t_vhdl_selovl is
  type r1 is record a : integer; end record;
  type r2 is record b : integer; end record;
  type r3 is record a : r1; end record;
  type r4 is record a : r2; end record;
  function f(a : integer := 1) return r3 is
  begin
    return (a => (a => a));
  end function;
  function f(a : integer := 2) return r4 is
  begin
    return (a => (b => a * 10));
  end function;
  type s1 is record x : integer; end record;
  type s2 is record x : boolean; end record;
  function g(n : integer) return s1 is
  begin
    return (x => n + 1);
  end function;
  function g(n : integer) return s2 is
  begin
    return (x => n > 0);
  end function;
begin
  process is
    variable i : integer;
    variable b : boolean;
  begin
    assert fun(true).field0 severity failure;
    assert not fun(false).field0 severity failure;
    assert fun(true).field1 = 11 severity failure;
    assert fun(false).field1 = 22 severity failure;
    assert fun(false) severity failure;
    assert f.a.a = 1 severity failure;
    assert f.a.b = 20 severity failure;
    assert f(3).a.a = 3 severity failure;
    assert f(4).a.b = 40 severity failure;
    -- Both results have an x; the target's type says which
    i := g(5).x;
    b := g(5).x;
    assert i = 6 severity failure;
    assert b severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
