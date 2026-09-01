-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

use std.textio.all;

package t_vhdl_pkgfileopen_pkg is
    -- A file declaration that names a file is open from the moment it is
    -- elaborated, with no call to FILE_OPEN anywhere (6.4.2.5). A package's
    -- is elaborated once, before anything runs.
    file logf : text open write_mode is "t_vhdl_pkgfileopen.log";

    procedure say (msg : string);
end package;

package body t_vhdl_pkgfileopen_pkg is
    procedure say (msg : string) is
        variable l : line;
    begin
        write(l, msg);
        writeline(logf, l);
    end procedure;
end package body;

use work.t_vhdl_pkgfileopen_pkg.all;

entity t_vhdl_pkgfileopen_one is
end entity;

architecture test of t_vhdl_pkgfileopen_one is
begin
    main : process is
    begin
        wait for 1 ns;
        say("one");
        wait;
    end process;
end architecture;

use std.textio.all;
use work.t_vhdl_pkgfileopen_pkg.all;

entity t_vhdl_pkgfileopen is
end entity;

architecture test of t_vhdl_pkgfileopen is
begin

    u1 : entity work.t_vhdl_pkgfileopen_one;

    check : process is
        file readf : text;
        variable l : line;
        variable got : string(1 to 3);
    begin
        wait for 2 ns;
        say("two");
        file_close(logf);
        file_open(readf, "t_vhdl_pkgfileopen.log", read_mode);

        readline(readf, l);
        read(l, got);
        assert got = "one" report "first line " & got severity failure;

        readline(readf, l);
        read(l, got);
        assert got = "two" report "second line " & got severity failure;

        assert endfile(readf) report "more than the two lines" severity failure;
        file_close(readf);

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
