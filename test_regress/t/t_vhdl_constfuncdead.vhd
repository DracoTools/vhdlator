-- DESCRIPTION: Verilator: VHDL helper naming an object of a unit that is not elaborated
--
-- t_vhdl_constfuncread's entity is analyzed beside this one but not
-- instantiated. Its helpers name "DATA_BITS", a constant worked out from a
-- function of the architecture, which no generic shows; placed in the unit
-- every module shares, they kept the unelaborated module's constant alive,
-- and its function was folded without having been prepared ("'return'
-- missed in LinkJump", GHDL gna issue2065 with its own top).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_constfuncdead is
end entity;

architecture a of t_vhdl_constfuncdead is
begin
  process
  begin
    report "constfuncdead done";
    std.env.finish;
  end process;
end architecture;
