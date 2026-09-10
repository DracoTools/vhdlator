-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A signal assignment writes a signal and a variable assignment a variable,
-- and neither writes what is of mode in. An out or inout parameter given no
-- class is a variable. A signal parameter is given a name denoting a
-- signal, and a variable parameter one denoting a variable, since what the
-- subprogram reads or writes through it is that object.

entity t_vhdl_objclass_bad is
  port (i : in bit; o : out bit);
end entity;

architecture a of t_vhdl_objclass_bad is
  type rec is record
    f : integer;
  end record;
  signal s : integer;
  signal sv : bit_vector(0 to 3);
  signal sr : rec;
  constant k : integer := 1;
  -- Legal
  procedure ok1 (signal x : out integer) is
  begin
    x <= 1;
  end procedure;
  procedure ok2 (x : inout integer) is
  begin
    x := 2;
  end procedure;
  function ok3 (signal x : bit) return bit is
  begin
    return x;
  end function;
  -- Illegal
  procedure bad1 (x : out integer) is
  begin
    x <= 1;
  end procedure;
  procedure bad2 (x : in integer) is
  begin
    x := 2;
  end procedure;
begin
  process
    variable v : integer;
    variable vr : rec;
  begin
    -- Legal
    ok1(s);
    ok2(v);
    sv(1) <= '1';
    sr.f <= 3;
    vr.f := 4;
    o <= ok3(i);
    -- Illegal
    k := 3;
    v <= 1;
    s := 2;
    i <= '0';
    ok1(v);
    ok2(s);
    ok2(k);
    o <= ok3('1');
    wait;
  end process;
end architecture;
