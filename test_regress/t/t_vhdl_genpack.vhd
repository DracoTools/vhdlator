-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A generic package is a template: each instantiation is a copy with the
-- generics fixed, analyzed from scratch (4.9). Also a user-defined operator,
-- and a subprogram parameter defaulting to a constant of its package.

package t_vhdl_genpack_pkg is
  generic (width : positive;
           type element_t;
           function widen (x : integer) return element_t);

  constant bits : natural := width;

  type vec_t is array (1 to width) of element_t;

  function fill (x : integer) return vec_t;
  function total return natural;
end package;

package body t_vhdl_genpack_pkg is
  function fill (x : integer) return vec_t is
    variable r : vec_t;
  begin
    for i in r'range loop
      r(i) := widen(x);
    end loop;
    return r;
  end function;

  function total return natural is
  begin
    return width * 2;
  end function;
end package body;

package t_vhdl_genpack_ops is
  constant base : integer := 10;
  type money is record
    cents : integer;
  end record;
  function "+" (l, r : money) return money;
  function scaled (m : money; by : integer := base) return integer;
end package;

package body t_vhdl_genpack_ops is
  function "+" (l, r : money) return money is
  begin
    return (cents => l.cents + r.cents);
  end function;
  function scaled (m : money; by : integer := base) return integer is
  begin
    return m.cents * by;
  end function;
end package body;

package t_vhdl_genpack_helpers is
  function same (x : integer) return integer;
end package;

package body t_vhdl_genpack_helpers is
  function same (x : integer) return integer is
  begin
    return x;
  end function;
end package body;

-- Instantiated as a design unit of its own
package t_vhdl_genpack_int is new work.t_vhdl_genpack_pkg
  generic map (width => 3, element_t => integer, widen => work.t_vhdl_genpack_helpers.same);

use std.env.all;
use work.t_vhdl_genpack_ops.all;

entity t_vhdl_genpack is
end entity;

architecture tb of t_vhdl_genpack is

  -- And instantiated again, differently, inside a declarative part
  package five is new work.t_vhdl_genpack_pkg
    generic map (width => 5, element_t => integer,
                 widen => work.t_vhdl_genpack_helpers.same);
  use five.all;

begin

  process
    variable v : five.vec_t;
    variable a, b, sum : money;
  begin
    assert work.t_vhdl_genpack_int.bits = 3 report "unit instance" severity failure;
    assert five.bits = 5 report "bits" severity failure;
    assert five.total = 10 report "total" severity failure;
    v := five.fill(7);
    assert v(1) = 7 and v(5) = 7 report "fill" severity failure;

    -- A user-defined operator is an ordinary function with an operator's name
    a := (cents => 3);
    b := (cents => 4);
    sum := a + b;
    assert sum.cents = 7 report "user-defined operator" severity failure;

    -- A parameter defaulting to a constant of its own package
    assert scaled(a) = 30 report "default from a package constant" severity failure;
    assert scaled(a, 2) = 6 report "explicit argument" severity failure;

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
