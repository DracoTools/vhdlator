-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_sensel is
end entity;

architecture test of t_vhdl_sensel is

    signal v : bit_vector(1 to 3) := "000";
    signal w : bit_vector(1 to 3) := "000";
    signal seen : natural := 0;

begin

    stim : process is
    begin
        wait for 1 ns;
        v(2) <= '1';                    -- must not wake the two below
        w(2) <= '1';
        wait for 1 ns;
        v(3) <= '1';
        wait for 1 ns;
        v(1) <= '1';
        wait;
    end process;

    -- A concurrent statement reading one element by a fixed index waits on
    -- that element alone (11.6), so this runs at time zero and at 3 ns
    p1 : assert (v(1) = '0' and now = 0 ns) or (v(1) = '1' and now = 3 ns)
        report "woke on another element" severity failure;

    -- A process with no sensitivity list follows the same rule
    p2 : process is
    begin
        assert now = 0 ns or now = 3 ns report "process woke on another element"
            severity failure;
        seen <= seen + 1;
        wait on w(1), v(1);
    end process;

    done : process is
    begin
        wait for 10 ns;
        assert v = "111" report "stimulus" severity failure;
        assert seen = 2 report "seen=" & integer'image(seen) severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
