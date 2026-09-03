-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_concdelta is
end entity;

architecture test of t_vhdl_concdelta is
    type rec is record
        el : bit;
    end record;
    type rec_array is array (1 downto 0) of rec;

    signal s : bit;
    signal t : bit;          -- one delta behind s
    signal u : bit;          -- two deltas behind s
    signal c : bit;          -- through a conditional assignment
    signal w : bit;          -- through a selected assignment
    signal arr : rec_array;
    signal held : boolean;   -- arr'stable, through an assignment

    -- One counter per process: a signal has one driver here
    signal watch_bad : natural := 0;
    signal until_bad : natural := 0;
    signal stim_bad : natural := 0;
    signal watch_ran : natural := 0;
    signal until_ran : natural := 0;

    -- Whether the whole chain still holds what it had before s changed
    function all_old (tv, uv, cv, wv : bit) return boolean is
    begin
        return tv = '0' and uv = '0' and cv = '0' and wv = '0';
    end function;
begin
    -- Each of these schedules an update rather than making one, so its
    -- target holds the old value through the delta in which s changed
    t <= s;
    u <= t;
    c <= '1' when s = '1' else '0';
    with s select w <= '1' when '1', '0' when others;
    held <= arr'stable;

    -- A process woken by an event on s sees what the others held before it
    watcher : process (s) is
    begin
        if s = '1' then
            if not all_old(t, u, c, w) then
                report "watcher saw the delta it should not" severity error;
                watch_bad <= watch_bad + 1;
            end if;
            watch_ran <= watch_ran + 1;
        end if;
    end process;

    -- The same for one woken by "wait until", which resumes in that delta too
    waiter : process is
    begin
        wait until s = '1';
        if not all_old(t, u, c, w) then
            report "waiter saw the delta it should not" severity error;
            until_bad <= until_bad + 1;
        end if;
        until_ran <= until_ran + 1;
        wait;
    end process;

    stim : process is
        procedure want (got : bit; expect : bit; what : string) is
        begin
            if got /= expect then
                report what & ": want " & bit'image(expect) & " got "
                    & bit'image(got) severity error;
                stim_bad <= stim_bad + 1;
            end if;
        end procedure;
    begin
        -- Before anything happens the chain is settled at its default
        wait for 1 ns;
        want(t, '0', "t at rest");
        want(u, '0', "u at rest");

        s <= '1';
        wait for 1 ns;
        -- A whole time step later every delta has passed
        want(t, '1', "t after a step");
        want(u, '1', "u after a step");
        want(c, '1', "c after a step");
        want(w, '1', "w after a step");

        -- 'STABLE answers about the delta the event fell in, and the
        -- assignment reading it puts the answer one delta further on
        assert held report "held should start true" severity error;
        arr(1).el <= '1' after 1 ns;
        wait for 1 ns;
        assert held report "held at the event's own delta" severity error;
        wait for 0 ns;
        assert not held report "held one delta after the event" severity error;
        wait for 0 ns;
        assert held report "held two deltas after the event" severity error;

        if watch_bad /= 0 or until_bad /= 0 or stim_bad /= 0 then
            report "%Error: a delta was lost" severity failure;
        end if;
        if watch_ran /= 1 or until_ran /= 1 then
            report "%Error: a process was not woken" severity failure;
        end if;
        report "*-* All Finished *-*";
        std.env.stop;
    end process;
end architecture;
