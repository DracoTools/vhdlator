-- DESCRIPTION: Verilator: VHDL attribute specification whose signature
-- matches no declaration of the name
--
-- A signature in an entity name list says which overload is decorated
-- (7.2); one that matches none of them names nothing.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_attrsig_bad is
end entity;

architecture a of t_vhdl_attrsig_bad is
  attribute cost : integer;
  function f(v : bit) return bit is
  begin
    return v;
  end function;
  function f(v : integer) return integer is
  begin
    return v;
  end function;
  attribute cost of f [boolean return boolean] : function is 3;
begin
end architecture;
