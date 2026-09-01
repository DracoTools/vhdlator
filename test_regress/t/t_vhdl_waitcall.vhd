-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_waitcall is
end entity;

architecture test of t_vhdl_waitcall is

    -- A function with a signal parameter is given the caller's own signal,
    -- so what it asks of it changes when the signal does (4.2.2.3)
    function falling (signal x : bit) return boolean is
    begin
        return x'event and x = '0';
    end function;

    function high (signal x : bit) return boolean is
    begin
        return x = '1';
    end function;

    signal clk : bit := '0';
    signal seen : natural := 0;

begin

    clkgen : clk <= not clk after 5 ns;

    counter : process is
    begin
        wait until falling(clk);
        seen <= seen + 1;
    end process;

    main : process is
    begin
        wait until falling(clk);
        assert now = 10 ns report "first falling at " & time'image(now) severity failure;
        wait until high(clk);
        assert now = 15 ns report "high at " & time'image(now) severity failure;
        wait for 21 ns;
        assert seen = 3 report "seen=" & integer'image(seen) severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
