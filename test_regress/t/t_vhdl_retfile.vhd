-- DESCRIPTION: Verilator: VHDL return of a value read from a file the function declared
--
-- A file a subprogram declares is closed as the subprogram is left (5.5.2),
-- which is after what it returns has been worked out: "return f1(f)" hands
-- the open file to another function.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

use std.textio.all;

entity t_vhdl_retfile is
end entity;

architecture test of t_vhdl_retfile is
    constant name : string := "t_vhdl_retfile.tmp";

    impure function first_word (file f : text) return integer is
        variable l : line;
        variable n : integer;
    begin
        if endfile(f) then
            return -1;
        end if;
        readline(f, l);
        read(l, n);
        return n;
    end function;

    impure function first_of (path : string) return integer is
        file f : text open read_mode is path;
    begin
        return first_word(f) + 1;
    end function;
begin

    process is
        file f : text;
        variable l : line;
    begin
        file_open(f, name, write_mode);
        write(l, integer'(41));
        writeline(f, l);
        file_close(f);
        assert first_of(name) = 42 severity failure;
        -- And again: the first call closed its file
        assert first_of(name) = 42 severity failure;

        file_open(f, name, write_mode);
        file_close(f);
        assert first_of(name) = 0 severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
