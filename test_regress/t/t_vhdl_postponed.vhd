-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

entity t_vhdl_postponed is
end entity;

architecture test of t_vhdl_postponed is
    signal x : natural;
    signal y : natural;
begin

    -- 11.3: "process (all)" is a process with a sensitivity list like any
    -- other, so it runs once at the start and then waits. Lowered as
    -- combinational logic instead, it would settle within one delta and a
    -- reader would never see the value the step began with.
    counter : process (all) is
        variable n : natural;
    begin
        n := n + 1;
        y <= x + n;
    end process;

    -- 10.3: a postponed process runs at the end of the last delta of a time
    -- step, so it sees the values the step settled at
    watch : postponed process is
    begin
        wait for 1 ns;
        assert y = 12 report "the postponed reader saw the settled value"
            severity failure;
        wait;
    end process;

    main : process is
    begin
        assert y = 0 report "nothing delivered yet" severity failure;
        wait for 0 ns;
        assert y = 1 report "once at the start" severity failure;
        x <= 10;
        wait for 0 ns;
        assert y = 1 report "one delta behind" severity failure;
        wait for 0 ns;
        assert y = 12 report "run again on the change" severity failure;
        wait for 0 ns;
        assert y = 12 report "and not again after that" severity failure;

        wait for 2 ns;

        report "*-* All Finished *-*";
        std.env.stop;
    end process;
end architecture;
