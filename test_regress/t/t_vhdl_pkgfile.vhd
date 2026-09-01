-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

use std.textio.all;

package t_vhdl_pkgfile_pkg is
    -- A file a package declares is one file for the whole design, not one
    -- per module that names the package: opened in one module, it is open
    -- in the next, and both write to it one after the other (4.3, 6.4.2.5)
    file logf : text;

    -- And the procedure that writes through it is one subprogram, however
    -- many modules call it
    procedure say (msg : string);
end package;

package body t_vhdl_pkgfile_pkg is
    procedure say (msg : string) is
        variable l : line;
    begin
        write(l, msg);
        writeline(logf, l);
    end procedure;
end package body;

use std.textio.all;
use work.t_vhdl_pkgfile_pkg.all;

entity t_vhdl_pkgfile_one is
end entity;

architecture test of t_vhdl_pkgfile_one is
begin
    main : process is
    begin
        wait for 1 ns;
        file_open(logf, "t_vhdl_pkgfile.log", write_mode);
        say("one");
        wait;
    end process;
end architecture;

use work.t_vhdl_pkgfile_pkg.all;

entity t_vhdl_pkgfile_two is
end entity;

architecture test of t_vhdl_pkgfile_two is
begin
    main : process is
    begin
        -- The other module opened it; this one just writes
        wait for 2 ns;
        say("two");
        wait;
    end process;
end architecture;

use std.textio.all;
use work.t_vhdl_pkgfile_pkg.all;

entity t_vhdl_pkgfile is
end entity;

architecture test of t_vhdl_pkgfile is
begin

    u1 : entity work.t_vhdl_pkgfile_one;
    u2 : entity work.t_vhdl_pkgfile_two;

    check : process is
        file readf : text;
        variable l : line;
        variable got : string(1 to 3);
    begin
        wait for 3 ns;
        file_close(logf);
        file_open(readf, "t_vhdl_pkgfile.log", read_mode);

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
