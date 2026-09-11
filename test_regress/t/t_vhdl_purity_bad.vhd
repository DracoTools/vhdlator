-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A pure function reads no signal, variable or file declared outside it and
-- calls no impure function (4.2.1). An attribute of an object asks about it
-- rather than reading it, so it does not count. References: nvc 1.22.1, GHDL
-- 7.0.0-dev, Questa 2025.2; GHDL's testsuite (gna/issue1691) needs NOW taken.

package pk is
  type pt is protected
    function get return integer;
  end protected;
  impure function imp return integer;
  signal ps : integer;
end package;

package body pk is
  type pt is protected body
    variable v : integer := 0;
    function get return integer is
    begin
      return v;  -- Bad: declared in the protected body, outside the function
    end function;
  end protected body;
  impure function imp return integer is
  begin
    return 1;
  end function;
end package body;

use work.pk.all;

entity t is
end entity;

architecture a of t is
  signal s : integer;
  signal bv : bit_vector(0 to 3);
  shared variable sv : pt;
  type ft is file of integer;
  file f : ft;
  constant k : integer := 3;
  function g(x : integer) return integer is
    variable lv : integer;
  begin
    lv := x + k + bv'length;  -- A parameter, a constant and an attribute
    if s'event then lv := 0; end if;  -- An attribute of the signal
    lv := s;  -- Bad
    lv := ps;  -- Bad
    lv := sv.get;  -- Bad
    lv := imp;  -- Bad
    lv := integer(now / 1 ns);  -- NOW is impure, but GHDL's own testsuite has this
    lv := <<signal .t.s : integer>>;  -- Bad
    if bv(0) = '1' then lv := 1; end if;  -- Bad
    if endfile(f) then lv := 2; end if;  -- Bad
    return lv;
  end function;
begin
  process
    variable pv : integer;
    function h return integer is
    begin
      return pv;  -- Bad
    end function;
    impure function ih return integer is
    begin
      return pv;
    end function;
  begin
    wait;
  end process;
end architecture;
