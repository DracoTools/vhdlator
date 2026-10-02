-- DESCRIPTION: Verilator: VHDL signal with several drivers, read before the process first suspends
--
-- Each driver starts with the signal's initial value and the signal with
-- what resolving them gives (14.7.3.2), so a process that reads it in its
-- first instant reads that.  It still runs in the first instant: what any
-- process assigns there is not seen until a delta later.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_firstread is
end entity;

architecture test of t_vhdl_firstread is
    signal ev : std_logic_vector(0 to 4) := "ZZ101";  -- two drivers
    signal one : std_logic := '1';                    -- two drivers
    signal x : integer := 0;                          -- written by "writer"
    signal y : integer := 0;                          -- written by "reader"
    signal done : boolean := false;
begin

    reader : process is
    begin
        -- Before anything has suspended
        assert now = 0 ns severity failure;
        assert ev(2 to 4) = "101" report "ev unresolved at the start" severity failure;
        assert one = '1' report "one unresolved at the start" severity failure;
        -- What the other process assigns in this same instant is not here yet
        assert x = 0 report "x seen too early" severity failure;
        y <= 7;
        ev(0) <= '1';
        wait for 0 ns;
        assert now = 0 ns severity failure;
        assert x = 1 severity failure;
        assert ev = "1Z101" severity failure;
        wait for 1 ns;
        ev(0) <= 'Z';
        one <= '0';
        wait for 1 ns;
        assert ev = "Z1101" report to_string(ev) severity failure;
        assert one = '0' severity failure;
        done <= true;
        wait;
    end process;

    writer : process is
    begin
        -- Nor is what the reader assigns seen here in the first instant
        assert y = 0 report "y seen too early" severity failure;
        x <= 1;
        wait for 0 ns;
        assert y = 7 severity failure;
        wait for 1 ns;
        ev(1) <= '1';
        one <= '0';
        wait until done;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
