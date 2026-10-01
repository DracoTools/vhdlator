-- DESCRIPTION: Verilator: VHDL string literal of no characters
--
-- "" is a STRING of no characters, wherever it is written: handed to WRITE,
-- held in a constant, compared, concatenated.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

use std.textio.all;

entity t_vhdl_nullstrlit is
end entity;

architecture test of t_vhdl_nullstrlit is
    function len (constant s : string) return natural is
    begin
        return s'length;
    end function;
begin

    process is
        variable l : line;
        constant e : string := "";
    begin
        write(l, string'(""));
        assert l'length = 0 severity failure;
        assert l.all = "" severity failure;
        write(l, string'("ab"));
        write(l, e);
        assert l.all = "ab" severity failure;
        write(l, string'("") & "x");
        assert l.all = "abx" severity failure;
        assert len("") = 0 and len(e) = 0 and len(string'("")) = 0 severity failure;
        assert e = "" and e & "q" = "q" severity failure;
        assert integer'image(7) & "" = "7" severity failure;
        deallocate(l);
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
