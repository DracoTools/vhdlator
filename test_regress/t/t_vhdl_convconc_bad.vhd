-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A conversion whose result is outside the subtype converted to is an error
-- (9.3.6), in a concurrent assertion as anywhere: "t(3)" with t 0 to 1 is not
-- folded to a value, and stops the model (GHDL gna issue618; nvc and Questa
-- refuse it at analysis, here a VHDLBOUNDS warning).

entity t_vhdl_convconc_bad is
end entity;

architecture behav of t_vhdl_convconc_bad is
  type t is range 0 to 1;
  constant c : t := 7 - 6;
begin
  assert c = t(3) report "c /= 3" severity note;
  process begin
    wait for 1 ns;
    report "not stopped" severity failure;
    wait;
  end process;
end architecture;
