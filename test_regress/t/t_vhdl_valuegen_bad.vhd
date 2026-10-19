-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- 'VALUE of text that is no value of the type is an error (16.2.3). Of a
-- generic's text, in a declaration, it is an error of elaboration, where nvc
-- and GHDL stop, though nothing reads the constant (GHDL gna bug0143).

entity t_vhdl_valuegen_bad is
  generic (s : string := "");
end entity;

architecture behav of t_vhdl_valuegen_bad is
  constant v : real := real'value(s);
begin
  process begin
    wait for 1 ns;
    report "not stopped" severity failure;
    wait;
  end process;
end architecture;
