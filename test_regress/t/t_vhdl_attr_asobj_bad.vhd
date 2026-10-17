-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An attribute's declaration is no object: "s.v" with "attribute s : rec"
-- names nothing that has a value (GHDL gna issue621; GHDL, nvc and Questa
-- refuse it)

entity t_vhdl_attr_asobj_bad is
end entity;

architecture a of t_vhdl_attr_asobj_bad is
  type rec is record
    v : natural;
  end record;
  attribute s : rec;
  function func return boolean is
  begin
    return s.v = 0;
  end function;
begin
end architecture;
