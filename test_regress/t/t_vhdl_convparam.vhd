-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A conversion may be written on the formal of a subprogram call, "p(f(b) =>
-- x)", where the parameter is written: what comes back out of it reaches the
-- actual through that conversion (6.5.7.1). The actual may carry a
-- conversion of its own, which converts the other way on the way in, so an
-- inout parameter of one type can be given an actual of another.

entity t_vhdl_convparam is
end entity;

package convpkg is
  procedure Twice(a : in integer; b : out integer);
  procedure Grow(a : in integer; b : inout integer);
  function  ToReal(i : in integer) return real;
  function  ToInt(r : in real) return integer;
end package;

package body convpkg is
  procedure Twice(a : in integer; b : out integer) is
  begin
    b := a * 2;
  end procedure;

  procedure Grow(a : in integer; b : inout integer) is
  begin
    b := b + a;
  end procedure;

  function ToReal(i : in integer) return real is
  begin
    return real(i) + 0.5;
  end function;

  function ToInt(r : in real) return integer is
  begin
    return integer(r * 2.0);
  end function;
end package body;

use work.convpkg.all;

architecture rtl of t_vhdl_convparam is
begin
  main : process
    variable x : real;
    variable n : integer;
  begin
    -- An out parameter, converted on its way to an actual of another type
    x := 0.0;
    Twice(10, ToReal(b) => x);
    assert x = 20.5 report "bad out conversion" severity error;

    -- An inout parameter: in through the actual's conversion, out through
    -- the formal's. ToInt(3.0) is 6, plus 4 is 10, and ToReal(10) is 10.5
    x := 3.0;
    Grow(4, ToReal(b) => ToInt(x));
    assert x = 10.5 report "bad inout conversion" severity error;

    -- The ordinary spelling, with no conversion anywhere, still works
    n := 0;
    Twice(7, n);
    assert n = 14 report "bad plain call" severity error;

    report "*-* All Finished *-*";
    std.env.stop;
    wait;
  end process;
end architecture;
