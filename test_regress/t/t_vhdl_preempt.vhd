-- DESCRIPTION: Verilator: VHDL driver's projected output waveform (10.5.2.2)
--
-- A transaction a process schedules preempts what its driver already holds:
-- with "transport", whatever is due at or after it; with inertial delay,
-- also what is due within the pulse rejection limit before it, except a run
-- of the value it carries.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_preempt is
end entity;

architecture test of t_vhdl_preempt is
    signal x, y1, y2, z : bit;
    signal s, r, c, sel : bit;
    signal v : bit_vector(3 downto 0);
    signal i : integer := 0;
begin

    -- Concurrent forms, each a process with a driver of its own
    y1 <= transport x after 450 ps;
    y2 <= reject 500 ps inertial x after 950 ps;
    z <= x after 300 ps;  -- inertial, rejecting anything shorter than 300 ps
    c <= x after 100 ps when sel = '1' else '0' after 100 ps;

    process is
    begin
        -- Transport: a later transaction deletes what is due at or after it
        x <= '1';                         -- 0
        wait for 1 ns;
        assert y1 = '1' severity failure;  -- 1000
        assert z = '1' severity failure;
        x <= transport '1' after 200 ps;
        x <= transport '0' after 100 ps;  -- deletes the '1' at 1200
        wait for 700 ps;
        assert x = '0' severity failure;  -- 1700
        assert y1 = '0' severity failure;
        wait for 300 ps;
        assert now = 2 ns severity failure;

        -- Inertial: x pulses for 200 ps, shorter than the 300 ps of z and
        -- the 500 ps rejection limit of y2, so neither sees it
        x <= '1';                         -- 2000 (z rose at 1300, fell at 2000)
        wait for 200 ps;
        x <= '0';                         -- 2200
        wait for 1500 ps;                 -- 3700
        assert z = '0' severity failure;
        assert z'last_event >= 1700 ps severity failure;
        assert y2 = '0' severity failure;
        -- A pulse as long as the rejection limit gets through y2 but is
        -- delayed by its full 950 ps
        x <= '1';                         -- 3700
        wait for 600 ps;
        x <= '0';                         -- 4300
        wait for 400 ps;                  -- 4700
        assert y2 = '1' severity failure;
        wait for 600 ps;                  -- 5300
        assert y2 = '0' severity failure;

        -- A transaction for now preempts everything pending
        s <= '1' after 2 ns;
        s <= '0';
        wait for 3 ns;
        assert s = '0' severity failure;

        -- Inertial keeps a run of the value it carries: the '1' due at 1 ns
        -- stays, since the '1' after it has the same value
        r <= '1' after 1 ns;
        r <= '1' after 2 ns;
        wait for 1 ns;
        assert r = '1' severity failure;
        wait for 2 ns;

        -- A vector has a driver for each element (14.7.2), so an inertial
        -- assignment keeps or deletes what is pending element by element:
        -- element 0 is already due to become '1' at 1 ns and keeps that,
        -- since '1' is what the new transaction gives it, while the other
        -- elements' pending '0' is of another value and is deleted.
        -- Transport keeps whatever is due before it.
        v <= "0001" after 1 ns, "0010" after 2 ns;
        v <= "1111" after 1500 ps;
        wait for 1 ns;
        assert v = "0001" severity failure;
        wait for 500 ps;
        assert v = "1111" severity failure;
        wait for 1 ns;
        assert v = "1111" severity failure;
        v <= transport "0001" after 1 ns, "0010" after 2 ns;
        v <= transport "0100" after 1500 ps;
        wait for 1 ns;
        assert v = "0001" severity failure;
        wait for 500 ps;
        assert v = "0100" severity failure;
        wait for 1 ns;
        assert v = "0100" severity failure;

        i <= 5 after 1 ns;
        i <= transport 7 after 2 ns;
        wait for 1 ns;
        assert i = 5 severity failure;
        wait for 1 ns;
        assert i = 7 severity failure;

        -- A conditional concurrent assignment is one driver too
        sel <= '1';
        x <= '1';
        wait for 50 ps;
        sel <= '0';                       -- the '1' due in 50 ps is preempted
        wait for 70 ps;
        assert c = '0' severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
