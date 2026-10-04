-- DESCRIPTION: Verilator: VHDL what a package instance's generics make visible
--
-- "use work.inst.all" makes visible what the instance declares, and its
-- generics with it: a generic type comes with "=" and "/=" (6.5.3), and a
-- generic subprogram "is <>" is the one visible where the package was
-- instantiated (6.5.6.2) -- here the enumeration's own TO_STRING. The design
-- unit using the instance does not use the package the type comes from
-- (GHDL gna issue2561).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_pkginstvis_gen is
    generic (
        type foo;
        constant nil, bar : foo;
        function to_string (a : foo) return string is <>
    );
    function consensus (a, b : foo) return foo;
end package;

package body t_vhdl_pkginstvis_gen is
    function consensus (a, b : foo) return foo is
    begin
        if a = bar and b = bar then
            return bar;
        end if;
        return nil;
    end function;
end package body;

package t_vhdl_pkginstvis_base is
    type foo is (A0, A1);
    constant nil : foo := A0;
    constant bar : foo := A1;
end package;

use work.t_vhdl_pkginstvis_base.all;
package t_vhdl_pkginstvis_inst is new work.t_vhdl_pkginstvis_gen
    generic map (foo => foo, nil => nil, bar => bar);

use work.t_vhdl_pkginstvis_inst.all;

entity t_vhdl_pkginstvis is
end entity;

architecture test of t_vhdl_pkginstvis is
    signal u, v : foo;
begin
    process is
    begin
        assert u = v and not (u /= v) severity failure;
        assert to_string(u) = "a0" severity failure;
        u <= bar;
        wait for 1 ns;
        assert u /= v severity failure;
        assert to_string(u) = "a1" and to_string(consensus(u, u)) = "a1" severity failure;
        assert consensus(u, v) = nil severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
