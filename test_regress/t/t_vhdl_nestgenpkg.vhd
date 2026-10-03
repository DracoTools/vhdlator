-- DESCRIPTION: Verilator: VHDL generic package declared in another, instantiated by its simple name
--
-- A use clause of the outer package makes the nested generic package
-- visible, and "package inner4 is new inner" instantiates it (4.9, 12.4).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_nestgenpkg_outer is
    package inner is
        generic (N : integer);
        constant twice : integer := 2 * N;
    end package;
end package;

entity t_vhdl_nestgenpkg is
end entity;

use work.t_vhdl_nestgenpkg_outer.all;

architecture test of t_vhdl_nestgenpkg is
    package inner4 is new inner generic map (4);
    package inner5 is new inner generic map (N => 5);
begin

    p1 : process is
    begin
        assert inner4.n = 4 severity failure;
        assert inner5.n = 5 severity failure;
        assert inner4.twice = 8 and inner5.twice = 10 severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
