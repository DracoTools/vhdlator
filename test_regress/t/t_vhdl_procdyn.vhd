-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_procdyn is
end entity;

architecture a of t_vhdl_procdyn is
    type int_vec is array (natural range <>) of integer;

    function ramp (n : natural) return int_vec is
        variable r : int_vec(1 to n);
    begin
        for i in r'range loop
            r(i) := i * 7;
        end loop;
        return r;
    end function;
begin
    -- A process declares its own variables, and one of them is as long as
    -- another says. The declarative part is elaborated once, before the
    -- process first runs, so the bounds are there for the first statement.
    watcher : process
        variable m : natural := 6;
        variable c : int_vec(1 to m);
        variable d : int_vec(1 to m) := ramp(m);
    begin
        assert c'length = 6 report "plain length" severity failure;
        assert d'length = 6 report "defaulted length" severity failure;
        assert d(1) = 7 and d(6) = 42 report "defaulted values" severity failure;

        for i in c'range loop
            c(i) := i * 3;
        end loop;
        assert c(1) = 3 and c(6) = 18 report "written values" severity failure;

        wait for 1 ns;
        assert c(6) = 18 report "kept across a wait" severity failure;

        report "*-* All Finished *-*";
        wait;
    end process;

    -- A second process, so nothing depends on there being only one
    other : process
        variable k : natural := 3;
        variable v : int_vec(1 to k);
    begin
        for i in v'range loop
            v(i) := i;
        end loop;
        assert v'length = 3 report "second process length" severity failure;
        wait;
    end process;
end architecture;
