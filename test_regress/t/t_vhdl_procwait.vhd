-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

entity t_vhdl_procwait is
end entity;

architecture test of t_vhdl_procwait is
    signal clk   : bit := '0';
    signal edges : natural := 0;

    procedure half_period (signal c : inout bit) is
    begin
        wait for 5 ns;
        c <= not c;
    end procedure;

    -- A wait two calls down still suspends the process that made them
    procedure full_period (signal c : inout bit) is
    begin
        half_period(c);
        half_period(c);
    end procedure;

    signal slow  : bit := '0';
    signal slows : natural := 0;
begin

    -- 10.2: a wait in a procedure suspends the process that called it, so a
    -- process whose only wait is in one still waits, and loops
    gen : process is
    begin
        half_period(clk);
    end process;

    gen_slow : process is
    begin
        full_period(slow);
    end process;

    count : process (clk) is
    begin
        if clk = '1' then
            edges <= edges + 1;
        end if;
    end process;

    count_slow : process (slow) is
    begin
        slows <= slows + 1;
    end process;

    main : process is
    begin
        wait for 52 ns;
        assert edges = 5 report "the generator kept running" severity failure;
        assert slows = 11 report "and so did the one two calls down" severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
    end process;
end architecture;
