-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- READ into a slice whose ends only running settles: as many characters as
-- the slice is long when it runs (16.4). The slice was read as a value, and
-- what was read was then assigned to that value (VESTS tc3202).
library ieee;
use ieee.std_logic_1164.all;
use std.textio.all;

entity t_vhdl_readslice is
end entity;

architecture test of t_vhdl_readslice is
begin
    process
        variable l : line;
        variable bv : bit_vector(1 to 12) := (others => '0');
        variable dn : bit_vector(11 downto 0) := (others => '0');
        variable sl : std_logic_vector(1 to 8) := (others => '0');
        variable n, m : natural;
        variable good : boolean;
    begin
        l := new string'("110 0101 1 A5");
        n := 3;
        read(l, bv(1 to n));
        assert bv = "110000000000" report "three bits" severity failure;
        m := 5;
        read(l, bv(n + 1 to n + 4), good);
        assert good report "good" severity failure;
        assert bv = "110010100000" report "four more" severity failure;
        read(l, dn(m downto m));
        assert dn = "000000100000" report "one, descending" severity failure;
        hread(l, sl(n - 2 to n + 5));
        assert sl = "10100101" report "hexadecimal" severity failure;
        read(l, bv(1 to n), good);
        assert not good report "nothing left to read" severity failure;
        assert bv = "110010100000" report "and nothing written" severity failure;

        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
