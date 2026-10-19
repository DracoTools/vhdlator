-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A LINE no one has given a string is null: textio writes to it as to an empty one, and so
-- after DEALLOCATE, but reading what it designates is an error (8.3, 16.4; GHDL gna bug0128).

use std.textio.all;

entity t_vhdl_linenull_bad is
end entity;

architecture a of t_vhdl_linenull_bad is
  procedure keep (variable x : inout line) is
  begin
  end procedure;
  procedure show (constant s : in string) is
  begin
    report "show '" & s & "'";
  end procedure;
begin
  process is
    variable l : line;
    variable x : line;
  begin
    assert l = null severity failure;
    write(l, string'("abc"));
    assert l /= null and l'length = 3 severity failure;
    writeline(output, l);
    deallocate(l);
    assert l = null severity failure;
    write(l, string'("def"));
    writeline(output, l);
    l := new string'("");
    assert l /= null and l'length = 0 severity failure;
    show(l.all);
    keep(x);
    show(x.all);
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
