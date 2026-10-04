-- DESCRIPTION: Verilator: VHDL aggregate over the range of a LINE's string
--
-- "(msg'range => ' ')" where msg points at a string as long as allocating
-- it made it: the aggregate is as long as that string (GHDL gna bug023).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

use std.textio.all;

entity t_vhdl_linerange is
end entity;

architecture test of t_vhdl_linerange is
    procedure blank (variable msg : inout line) is
    begin
        msg.all := (msg'range => ' ');
    end procedure;

    function stars (s : string) return string is
        variable r : string(s'range);
    begin
        r := (s'range => '*');
        return r;
    end function;
begin
    process is
        variable l : line := new string'("abcdef");
    begin
        blank(l);
        assert l.all = "      " severity failure;
        assert l'length = 6 severity failure;
        l.all := (l'reverse_range => '-');
        assert l.all = "------" severity failure;
        deallocate(l);
        l := new string'("xy");
        blank(l);
        assert l.all = "  " severity failure;
        assert stars("abc") = "***" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
