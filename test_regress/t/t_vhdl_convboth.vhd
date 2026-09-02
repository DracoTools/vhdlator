-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_convboth is
end entity;

architecture test of t_vhdl_convboth is
    signal s : integer := 0;
    signal t : integer := 0;
begin

    -- A conversion written on the formal converts what the port drives on
    -- its way out to the actual (6.5.7.1). One written on the actual as
    -- well does not stop that: the association is still with the signal
    -- inside it, and that is what the port drives.
    b1 : block is
        port (p : inout real := 0.0);
        port map (integer(p) => real(s));
    begin
        p <= 1.0, 2.5 after 1 ns;
    end block;

    -- The same with nothing written on the actual, which worked before
    b2 : block is
        port (q : out real := 0.0);
        port map (integer(q) => t);
    begin
        q <= 3.0, 4.5 after 1 ns;
    end block;

    chk : process is
    begin
        wait for 0 ns;
        assert s = 1 report "s at 0: " & integer'image(s) severity failure;
        assert t = 3 report "t at 0: " & integer'image(t) severity failure;

        wait for 1 ns;
        assert s = 3 report "s at 1: " & integer'image(s) severity failure;
        assert t = 5 report "t at 1: " & integer'image(t) severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
