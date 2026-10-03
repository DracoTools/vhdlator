-- DESCRIPTION: Verilator: VHDL 'ACTIVE of S'STABLE and S'QUIET (16.2.4)
--
-- An implicit signal S'STABLE(T) has a transaction, FALSE, in each cycle in
-- which S has an event, and one more, TRUE, T after the last of them: in
-- the next delta when T is zero. S'QUIET(T) is the same for transactions.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_implactive is
end entity;

architecture test of t_vhdl_implactive is
    signal i1 : natural;
    signal x : integer;
begin

    process is
    begin
        assert not x'stable'active severity failure;
        assert not x'quiet'active severity failure;
        assert not x'quiet(1 ns)'active severity failure;

        i1 <= 4;
        wait for 0 ns;
        assert i1'stable'active severity failure;   -- FALSE now
        assert i1'stable(2 ns)'active severity failure;
        wait for 0 ns;
        assert i1'stable'active severity failure;   -- TRUE a delta later
        assert not i1'stable(2 ns)'active severity failure;
        wait for 0 ns;
        assert not i1'stable'active severity failure;
        wait for 2 ns;
        assert i1'stable(2 ns)'active severity failure;  -- TRUE 2 ns later
        assert not i1'stable'active severity failure;
        wait for 0 ns;
        assert not i1'stable(2 ns)'active severity failure;

        -- A later event cancels the TRUE an earlier one scheduled
        i1 <= 5;
        wait for 1 ns;
        i1 <= 6;
        wait for 1 ns;
        assert not i1'stable(2 ns)'active severity failure;
        wait for 1 ns;
        assert i1'stable(2 ns)'active severity failure;

        -- A transaction that is no event is seen by 'QUIET only
        i1 <= 6;
        wait for 0 ns;
        assert i1'quiet'active severity failure;
        assert not i1'stable'active severity failure;
        wait for 0 ns;
        assert i1'quiet'active severity failure;
        wait for 0 ns;
        assert not i1'quiet'active severity failure;

        wait for 1 ns;
        assert not x'quiet(1 ns)'active severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
