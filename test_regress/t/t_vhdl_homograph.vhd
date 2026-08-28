-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package own is
    -- A type of a name VHDL-2008 also gives STD.STANDARD (16.3)
    type boolean_vector is array (natural range <>) of boolean;
    function count (v : boolean_vector) return natural;
end package;

package body own is
    function count (v : boolean_vector) return natural is
        variable n : natural := 0;
    begin
        for i in v'range loop
            if v(i) then n := n + 1; end if;
        end loop;
        return n;
    end function;
end package body;

use work.own.all;

entity t_vhdl_homograph is
end entity;

architecture test of t_vhdl_homograph is
    -- The design's own declaration is the one meant, in the declaration
    -- below and in the call, which have to agree. 12.4 c) makes the name
    -- ambiguous instead, and both nvc 1.22.1 and GHDL 7.0.0-dev refuse it
    -- on that ground; taking the design's own declaration accepts a design
    -- they reject and never changes one they accept, since STD.STANDARD's
    -- is still reachable by its selected name.
    signal v : boolean_vector(0 to 3) := (true, false, true, true);
begin

    process is
    begin
        wait for 1 ns;
        assert count(v) = 3 report "count" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
