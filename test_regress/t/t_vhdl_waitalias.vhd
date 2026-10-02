-- DESCRIPTION: Verilator: VHDL wait on a signal handed to a function by alias
--
-- "wait until f(a)" waits on the signal a function is handed (10.2), and an
-- alias of part of a signal is that part of the signal (6.6.2).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_waitalias is
end entity;

architecture test of t_vhdl_waitalias is
    signal bus_s : bit_vector(0 to 9) := (others => '0');
    alias low is bus_s(0 to 4);
    alias high is bus_s(5 to 9);
    alias whole is bus_s;
    alias again is high;
    signal woke_high, woke_whole, woke_again : time := 0 ns;

    function set (signal s : bit_vector) return boolean is
    begin
        return s(s'left) = '1';
    end function;
begin

    -- Each of these would wake only at its timeout if the alias were not
    -- waited on
    process is
    begin
        wait until set(high) for 50 ns;
        woke_high <= now;
        wait;
    end process;

    process is
    begin
        wait until set(whole) for 50 ns;
        woke_whole <= now;
        wait;
    end process;

    process is
    begin
        wait until set(again) for 50 ns;
        woke_again <= now;
        wait;
    end process;

    process is
    begin
        wait for 3 ns;
        bus_s(5) <= '1';
        wait for 4 ns;
        bus_s(0) <= '1';
        wait for 10 ns;
        assert woke_high = 3 ns report "alias of a slice" severity failure;
        assert woke_again = 3 ns report "alias of an alias" severity failure;
        assert woke_whole = 7 ns report "alias of the whole" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
