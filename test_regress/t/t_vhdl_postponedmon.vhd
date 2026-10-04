-- DESCRIPTION: Verilator: VHDL postponed process that only reads its signals
--
-- A postponed process runs once at the end of a time step in which one of
-- its signals changed, after the last delta cycle, and reads the values the
-- step ended with (11.3, 14.7.5.3): a change undone within the step is
-- seen once, as its last value. It ran a step late with the step before's
-- values (found with GHDL gna issue2561's monitor).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_postponedmon is
end entity;

architecture test of t_vhdl_postponedmon is
    signal x, z, d1, d2 : bit;
    signal runs : integer := 0;
begin
    x <= '0', '1' after 3 ns;
    z <= '0', '1' after 4 ns, '0' after 10 ns;
    -- Two deltas in a row at 6 ns: '1', then '0' again
    d1 <= '1' after 6 ns, '0' after 7 ns;
    d2 <= d1;

    log : postponed process (all) is
        variable n : integer := 0;
    begin
        n := n + 1;
        case n is
            when 1 => assert now = 0 ns and x = '0' and z = '0' severity failure;
            when 2 => assert now = 3 ns and x = '1' and z = '0' severity failure;
            when 3 => assert now = 4 ns and x = '1' and z = '1' severity failure;
            -- d1 then d2 change in successive deltas of 6 ns: one run
            when 4 => assert now = 6 ns and d1 = '1' and d2 = '1' severity failure;
            when 5 => assert now = 7 ns and d1 = '0' and d2 = '0' severity failure;
            when 6 => assert now = 10 ns and z = '0' severity failure;
            when others => assert false severity failure;
        end case;
        report "run " & integer'image(n) & " at " & time'image(now);
    end process;

    check : process is
    begin
        wait for 20 ns;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
