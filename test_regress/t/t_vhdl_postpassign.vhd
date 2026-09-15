-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

entity t_vhdl_postpassign is
end entity;

architecture test of t_vhdl_postpassign is
    signal x : natural := 0;
    signal y : natural := 0;
    signal z : natural := 0;
    signal seen : natural := 0;
begin

    -- A reader of y, so that y is in a sensitivity list: what the postponed
    -- assignment writes has to reach the scheduler as an ordinary value
    watcher : process (y) is
    begin
        seen <= seen + 1;
    end process;

    -- 11.3: a postponed concurrent assignment runs at the end of the last
    -- delta of the time step. It is an error for one to schedule a
    -- transaction that would start another delta, which is why what it
    -- writes takes effect where it is written. GHDL 7.0.0-dev runs this and
    -- Questa 2025.2 runs it with a suppressible error; nvc 1.22.1 stops. All
    -- three deliver the value asserted below, which is what is tested here:
    -- Verilator counts no deltas, so the violation is not observable.
    postponed y <= x;

    -- The same with a delay, which schedules a transaction of its own
    postponed z <= x after 1 ns;

    main : process is
    begin
        x <= 5;
        wait for 1 ns;
        assert y = 5 report "the postponed assignment delivered its value" severity failure;
        assert seen > 0 report "and the reader woke on it" severity failure;
        wait for 2 ns;
        assert z = 5 report "and one with a delay keeps its delay" severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
    end process;
end architecture;
