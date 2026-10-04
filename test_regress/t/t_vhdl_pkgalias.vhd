-- DESCRIPTION: Verilator: VHDL alias of a package
--
-- "alias p is work.pkg" makes p another name for the package (6.6.3), so
-- "p.c" is the package's constant and "p.f" its function, in a package
-- and in an architecture (GHDL gna issue2473).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_pkgalias_pkg is
    constant c : string := ('a', 'b');
    constant n : integer := 7;
    function twice (x : integer) return integer;
end package;

package body t_vhdl_pkgalias_pkg is
    function twice (x : integer) return integer is
    begin
        return 2 * x;
    end function;
end package body;

use work.t_vhdl_pkgalias_pkg;

package t_vhdl_pkgalias_user is
    alias p is t_vhdl_pkgalias_pkg;
    constant e : string := p.c;
    constant m : integer := p.twice(p.n);
end package;

use work.t_vhdl_pkgalias_user.all;

entity t_vhdl_pkgalias is
end entity;

architecture test of t_vhdl_pkgalias is
    alias q is work.t_vhdl_pkgalias_pkg;
    constant f : string := q.c;
begin
    process is
    begin
        assert e = "ab" and f = "ab" severity failure;
        assert m = 14 and q.twice(3) = 6 and q.n = 7 severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
