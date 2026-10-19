-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A slice of what a LINE designates that is not null and does not lie inside it stops the
-- run (8.5): after READ took four of thirteen characters, nine are left, from 1 (16.4; GHDL
-- gna issue2193, where nvc and Questa stop).

use std.textio.all;

entity t_vhdl_lineslice_bad is
end entity;

architecture a of t_vhdl_lineslice_bad is
begin
  process is
    variable l : line;
    variable s : string(1 to 4);
  begin
    l := new string'("hereisastring");
    read(l, s);
    report s;
    report l.all(1 to 9);
    report l.all(5 to 13);
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
