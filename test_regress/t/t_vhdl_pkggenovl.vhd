-- DESCRIPTION: Verilator: VHDL generic subprograms of one package mapped to
-- overloads of one name
--
-- "generic map (f, f)" binds "generic_f(b : bit) return boolean" and
-- "generic_f(b : boolean) return bit" each to the f whose profile conforms
-- to its own (6.5.7.2), so the copy declares one generic_f of each profile
-- rather than two of both (GHDL gna issue439). The profiles may name a
-- generic type of the same package, which stands for what the instance
-- gave it: "to_int => conv, from_int => conv" picks a different conv for
-- each.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package p is
  generic (
    function generic_f(b : bit) return boolean;
    function generic_f(b : boolean) return bit);
  function f(b : bit) return boolean;
  function f(b : boolean) return bit;
end package;

package body p is
  function f(b : bit) return boolean is
  begin
    return generic_f(b);
  end function;
  function f(b : boolean) return bit is
  begin
    return generic_f(b);
  end function;
end package body;

package pt is
  generic (
    type t;
    function to_int(x : t) return integer;
    function from_int(x : integer) return t;
    procedure step_t(x : inout t);
    procedure step_i(x : inout integer));
  function twice(x : t) return integer;
  function back(x : integer) return t;
end package;

package body pt is
  function twice(x : t) return integer is
    variable v : t := x;
    variable n : integer := to_int(x);
  begin
    step_t(v);
    step_i(n);
    return to_int(v) + n;
  end function;
  function back(x : integer) return t is
  begin
    return from_int(x);
  end function;
end package body;

entity t_vhdl_pkggenovl is
end entity;

architecture a of t_vhdl_pkggenovl is
  function f(b : bit) return boolean is
  begin
    return b = '1';
  end function;
  function f(b : boolean) return bit is
  begin
    if b then
      return '0';
    end if;
    return '1';
  end function;
  package q is new work.p generic map (f, f);

  function conv(x : bit_vector(3 downto 0)) return integer is
    variable n : integer := 0;
  begin
    for i in x'range loop
      n := n * 2;
      if x(i) = '1' then
        n := n + 1;
      end if;
    end loop;
    return n;
  end function;
  function conv(x : integer) return bit_vector is
    variable v : bit_vector(3 downto 0);
    variable n : integer := x;
  begin
    for i in 0 to 3 loop
      if n mod 2 = 1 then
        v(i) := '1';
      else
        v(i) := '0';
      end if;
      n := n / 2;
    end loop;
    return v;
  end function;
  procedure step(x : inout bit_vector(3 downto 0)) is
  begin
    x := x(2 downto 0) & '0';
  end procedure;
  procedure step(x : inout integer) is
  begin
    x := x + 100;
  end procedure;
  subtype nib is bit_vector(3 downto 0);
  package r is new work.pt
    generic map (t => nib, to_int => conv, from_int => conv, step_t => step, step_i => step);
begin
  process is
  begin
    assert q.f('1') severity failure;
    assert not q.f('0') severity failure;
    assert q.f(true) = '0' severity failure;
    assert q.f(false) = '1' severity failure;
    -- 3 shifted is 6, and 3 stepped is 103
    assert r.twice("0011") = 109 severity failure;
    assert r.back(5) = "0101" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
