-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- There is no value to the right of a type's last one (16.2.2): 'RIGHTOF of
-- it is an error when evaluated, as 'SUCC is (GHDL gna ticket65; nvc stops
-- the run, Questa refuses it at analysis).

entity t_vhdl_rightof_bad is
end entity;

architecture behav of t_vhdl_rightof_bad is
begin
  process
    type enum_t is (value1, value2);
    variable var : enum_t := value1;
  begin
    var := enum_t'rightof(var);
    report "rightof value1 is " & enum_t'image(var);
    var := enum_t'rightof(var);
    report "not stopped" severity failure;
    wait;
  end process;
end architecture;
