-- DESCRIPTION: Verilator: VHDL concurrent assignment to a slice whose ends a signal gives
--
-- "r(7 downto 8 - n) <= x(n - 1 downto 0)" as a concurrent statement, n a
-- signal: the statement is the process it is defined to be (11.6), the
-- sequential assignment woken by what it reads, the ends included. As a
-- continuous assignment its part-select had a width only running knew,
-- which Verilog has no form for (GHDL gna issue522 rotates this way).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_concslicerun is
end entity;

architecture test of t_vhdl_concslicerun is
    signal x : std_logic_vector(7 downto 0) := "10010111";
    signal r : std_logic_vector(7 downto 0) := (others => '0');
    signal n : integer := 3;
begin
    r(7 downto 8 - n) <= x(n - 1 downto 0);

    process is
    begin
        wait for 1 ns;
        assert r = "11100000" severity failure;
        n <= 5;
        wait for 1 ns;
        assert r = "10111000" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
