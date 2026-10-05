-- DESCRIPTION: Verilator: VHDL methods of a protected type that differ only
-- in their result, chosen by the expression around the call
--
-- "x.g(x.f + 25.0)" calls the f that returns REAL, since only that one
-- makes "+" with a REAL literal mean anything; "check(id, params.get(5),
-- 17)" calls the get that returns INTEGER, which is what the overload of
-- check taking an INTEGER beside 17 wants (12.5; nvc parse issue1614 and
-- osvvm8, the shape of OSVVM's ModelParametersPkg).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_protovl is
end entity;

architecture a of t_vhdl_protovl is
  type t is protected
    impure function f return real;
    impure function f return integer;
    impure function g(x : real) return integer;
    impure function g(x : integer) return integer;
    impure function get(index : natural) return integer;
    impure function get(index : natural) return boolean;
    impure function get(index : natural) return bit_vector;
  end protected;

  type t is protected body
    impure function f return real is
    begin
      return 1.5;
    end function;
    impure function f return integer is
    begin
      return 7;
    end function;
    impure function g(x : real) return integer is
    begin
      return integer(x * 2.0);
    end function;
    impure function g(x : integer) return integer is
    begin
      return x + 1000;
    end function;
    impure function get(index : natural) return integer is
    begin
      return index * 3;
    end function;
    impure function get(index : natural) return boolean is
    begin
      return index > 3;
    end function;
    impure function get(index : natural) return bit_vector is
    begin
      return "1010";
    end function;
  end protected body;

  shared variable x : t;

  impure function check(id : integer; got, want : integer) return integer is
  begin
    if got = want then
      return 1;
    end if;
    return 0;
  end function;
  impure function check(id : integer; got, want : boolean) return integer is
  begin
    return 2;
  end function;
begin
  process is
    variable y : integer;
  begin
    -- 2 * (1.5 + 25.0) = 53
    y := x.g(x.f + 25.0);
    assert y = 53 severity failure;
    y := x.g(x.f + 1);
    assert y = 1008 severity failure;
    assert check(0, x.get(5), 15) = 1 severity failure;
    assert check(0, x.get(5), true) = 2 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
