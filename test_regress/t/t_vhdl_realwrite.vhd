-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- TEXTIO's WRITE of a REAL with DIGITS left at 0 writes the value in
-- exponential form (16.4), which nvc 1.22.1 and Questa 2025.2 both write as
-- "7.142857e-01". From GHDL gna issue20, whose round trip through a file
-- fails on nvc, Questa and here alike for the digits it drops.

use std.textio.all;

entity t_vhdl_realwrite is
end entity;

architecture a of t_vhdl_realwrite is
  procedure check (v : real; want : string) is
    variable l : line;
  begin
    write(l, v);
    assert l.all = want report "got [" & l.all & "] want [" & want & "]" severity failure;
    deallocate(l);
  end procedure;
begin
  process
    variable l : line;
  begin
    check(5.0 / 7.0, "7.142857e-01");
    check(-1234.5, "-1.234500e+03");
    check(0.0, "0.000000e+00");
    check(1.0e-20, "1.000000e-20");
    check(3.0e30, "3.000000e+30");
    check(100.0, "1.000000e+02");
    write(l, 2.5, right, 14);
    assert l.all = "  2.500000e+00" report "field [" & l.all & "]" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
