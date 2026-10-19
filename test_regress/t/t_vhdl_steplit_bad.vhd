-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- 'RIGHTOF of a literal, written in a statement, stepping past the end of its type stops the
-- run when the statement runs (16.2.2; GHDL gna ticket65).

entity t_vhdl_steplit_bad is
end entity;

architecture a of t_vhdl_steplit_bad is
begin
  process is
    type enum_t is (value1, value2);
    variable var : enum_t;
  begin
    var := enum_t'rightof(value1);
    report "rightof value1 is " & enum_t'image(var);
    var := enum_t'leftof(value2);
    report "leftof value2 is " & enum_t'image(var);
    var := enum_t'rightof(value2);
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
