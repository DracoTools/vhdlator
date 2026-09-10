-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A function computes a value from what it is given, so its parameters are
-- of mode in and none is a variable; a procedure's are in, out or inout,
-- since buffer and linkage describe ports.

entity t_vhdl_parammode_bad is
end entity;

architecture a of t_vhdl_parammode_bad is
  function f1 (a : inout integer) return boolean is
  begin
    return true;
  end function;
  function f2 (variable a : integer) return boolean is
  begin
    return true;
  end function;
  procedure p1 (c : linkage integer) is
  begin
  end procedure;
  procedure p2 (c : buffer integer) is
  begin
  end procedure;
begin
end architecture;
