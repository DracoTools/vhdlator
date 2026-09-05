-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_rangedir is
end entity;

architecture a of t_vhdl_rangedir is
    type int_vec is array (natural range <>) of integer;

    -- A bound worked out by calling something, so neither end is a number
    -- where the array is declared
    function twice (n : natural) return natural is
    begin
        return n * 2;
    end function;

    -- Written "to", and handed back through a type nothing constrains, so
    -- the value has to carry the bounds it was declared with
    function upward return int_vec is
        variable r : int_vec(1 to twice(3));
    begin
        for i in r'range loop
            r(i) := i * 10;
        end loop;
        return r;
    end function;

    -- The same, written "downto"
    function downward return int_vec is
        variable r : int_vec(twice(3) downto 1);
    begin
        for i in r'range loop
            r(i) := i * 100;
        end loop;
        return r;
    end function;
begin
    process
        variable u : int_vec(1 to 6);
        variable d : int_vec(6 downto 1);
    begin
        u := upward;
        assert u'length = 6 report "upward length" severity failure;
        assert u'ascending report "upward direction" severity failure;
        assert u(1) = 10 and u(6) = 60 report "upward values" severity failure;

        d := downward;
        assert d'length = 6 report "downward length" severity failure;
        assert not d'ascending report "downward direction" severity failure;
        assert d(6) = 600 and d(1) = 100 report "downward values" severity failure;

        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
