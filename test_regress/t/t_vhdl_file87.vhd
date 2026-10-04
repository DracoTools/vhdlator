-- DESCRIPTION: Verilator: VHDL file declaration in the VHDL-87 form
--
-- "file f : text is out "name"" and "is in "name"" are how VHDL-87 wrote
-- a file's mode, meaning WRITE_MODE and READ_MODE. nvc and Questa (with a
-- warning) still take them in later revisions, and GHDL's testsuite writes
-- them (gna bug096).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

use std.textio.all;

entity t_vhdl_file87 is
end entity;

architecture test of t_vhdl_file87 is
begin
    process is
        procedure put is
            file f : text is out "t_vhdl_file87.tmp";
            variable l : line;
        begin
            write(l, string'("first"));
            writeline(f, l);
            write(l, 42);
            writeline(f, l);
        end procedure;

        procedure get is
            file f : text is in "t_vhdl_file87.tmp";
            variable l : line;
            variable n : integer;
        begin
            readline(f, l);
            assert l.all = "first" severity failure;
            readline(f, l);
            read(l, n);
            assert n = 42 severity failure;
            assert endfile(f) severity failure;
        end procedure;
    begin
        put;
        get;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
