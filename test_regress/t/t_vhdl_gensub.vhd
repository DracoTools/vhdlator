-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A generic subprogram is a template: it is never called as it stands, only
-- through an instance made with "is new" (4.4, 6.5.5). Its generics may be
-- values, types or other subprograms, and one written "is <>" takes whatever
-- is visible where the instance is made.

package t_vhdl_gensub_pkg is

  function adder generic (n : integer) (x : integer) return integer;

  function apply generic (type t;
                          function op (l, r : t) return t is <>)
                         (a, b : t) return t;

  procedure bump generic (type t;
                          function "+" (l, r : t) return t is <>;
                          step : t) (signal s : inout t);

end package;

package body t_vhdl_gensub_pkg is

  function adder generic (n : integer) (x : integer) return integer is
  begin
    return x + n;
  end function;

  function apply generic (type t;
                          function op (l, r : t) return t is <>)
                         (a, b : t) return t is
  begin
    return op(a, b);
  end function;

  procedure bump generic (type t;
                          function "+" (l, r : t) return t is <>;
                          step : t) (signal s : inout t) is
  begin
    s <= s + step;
  end procedure;

end package body;

use std.env.all;
use work.t_vhdl_gensub_pkg.all;

entity t_vhdl_gensub is
end entity;

architecture tb of t_vhdl_gensub is

  -- The body is in a package body this instantiation cannot see
  function add1 is new adder generic map (1);
  function add10 is new adder generic map (n => 10);

  -- "op" is left to whatever "+" is visible here, for each type in turn
  function apply_int is new apply generic map (t => integer, op => "+");
  function apply_real is new apply generic map (t => real, op => "+");

  procedure bump_int is new bump generic map (t => integer, step => 3);

  -- A generic subprogram declared here, and recursive: the call to its own
  -- name inside the body means the instance
  function fact generic (type t;
                         function "*" (l, r : t) return t is <>;
                         function "-" (l, r : t) return t is <>;
                         function "<" (l, r : t) return boolean is <>;
                         one : t) (n : t) return t is
  begin
    if n < one then
      return one;
    else
      return n * fact(n - one);
    end if;
  end function;

  function fact_int is new fact generic map (t => integer, one => 1);

  signal counter : integer := 0;

begin

  process is
  begin
    assert add1(1) = 2 report "a value generic" severity failure;
    assert add10(1) = 11 report "a value generic by name" severity failure;

    assert apply_int(2, 3) = 5 report "a type generic" severity failure;
    assert apply_real(0.5, 0.25) = 0.75 report "the same one on real" severity failure;

    assert fact_int(5) = 120 report "recursion through the instance" severity failure;

    bump_int(counter);
    wait for 1 ns;
    assert counter = 3 report "a signal parameter of an instance" severity failure;
    bump_int(counter);
    wait for 1 ns;
    assert counter = 6 report "and again" severity failure;

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
