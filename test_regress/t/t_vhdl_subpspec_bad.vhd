-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Only a function overloads an operator, and only an operator of the
-- language, written without spaces.

entity t_vhdl_subpspec_bad is
end entity;

architecture a of t_vhdl_subpspec_bad is
  function "+" (l, r : bit) return bit is
  begin
    return l xor r;
  end function;
  function "?=" (l, r : bit) return bit is
  begin
    return l xnor r;
  end function;
  procedure "+" (x : in integer) is
  begin
  end procedure;
  function " + " (l, r : integer) return integer is
  begin
    return l;
  end function;
  function "eor" (l, r : bit) return bit is
  begin
    return l;
  end function;
begin
end architecture;
