-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_rtlen is
end entity;

architecture a of t_vhdl_rtlen is
    type int_vec is array (natural range <>) of integer;

    signal howmany : natural := 4;

    -- Impure, so its answer is whatever the signal holds when it is called,
    -- and no elaboration can work it out
    impure function n_items return natural is
    begin
        return howmany;
    end function;

    -- A bound written as a call to one of those
    impure function counted return int_vec is
        variable r : int_vec(1 to n_items);
    begin
        for i in r'range loop
            r(i) := i * 10;
        end loop;
        return r;
    end function;

    -- A bound naming a constant declared from one of those
    impure function named return int_vec is
        constant m : natural := n_items;
        variable r : int_vec(0 to m - 1);
    begin
        for i in r'range loop
            r(i) := i + 100;
        end loop;
        return r;
    end function;

    -- A pure function is not one of those: asked the same thing twice it
    -- answers the same way, so elaboration works the bound out and the array
    -- stays a plain one of a length the emitted code knows
    function twice (n : natural) return natural is
    begin
        return n * 2;
    end function;

    -- Nor is this one, whatever it was declared: "impure" is a promise the
    -- language needs, not a description of the body, and a body that reads
    -- only what it was handed answers the same way every time
    impure function thrice (n : natural) return natural is
    begin
        return n * 3;
    end function;
begin
    process
        variable c : int_vec(1 to 4);
        variable d : int_vec(0 to 3);
        variable e : int_vec(1 to twice(3));
        variable f : int_vec(1 to thrice(3));
    begin
        c := counted;
        assert c'length = 4 report "counted length" severity failure;
        assert c(1) = 10 and c(4) = 40 report "counted values" severity failure;

        d := named;
        assert d'length = 4 report "named length" severity failure;
        assert d(0) = 100 and d(3) = 103 report "named values" severity failure;

        assert e'length = 6 report "pure bound length" severity failure;
        for i in e'range loop
            e(i) := i;
        end loop;
        assert e(1) = 1 and e(6) = 6 report "pure bound values" severity failure;

        assert f'length = 9 report "nominally impure bound length" severity failure;
        for i in f'range loop
            f(i) := i * 2;
        end loop;
        assert f(1) = 2 and f(9) = 18 report "nominally impure bound values"
            severity failure;

        -- The same call, a different length, because the signal moved
        howmany <= 2;
        wait for 1 ns;
        assert counted'length = 2 report "shrunk length" severity failure;
        assert named'length = 2 report "shrunk named length" severity failure;

        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
