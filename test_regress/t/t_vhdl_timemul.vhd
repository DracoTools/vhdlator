-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_timemul is
end entity;

architecture test of t_vhdl_timemul is

    constant C_BIT_PERIOD : time := 100 ns;
    constant C_COUNT      : integer := 3;

begin

    main : process is
        variable t : time;
        variable r : real := 2.5;
    begin
        -- STANDARD declares "*" over a physical type twice, once counting in
        -- INTEGER and once in the universal integer, and both read the same
        -- here (16.3): a product of literals is either
        t := 3 * 13 * C_BIT_PERIOD;
        assert t = 3900 ns report "literal product" severity failure;

        -- An INTEGER object leaves only the one reading
        t := C_COUNT * C_BIT_PERIOD;
        assert t = 300 ns report "integer count" severity failure;

        t := C_BIT_PERIOD * 2;
        assert t = 200 ns report "the other way about" severity failure;

        -- The same for the real ones
        t := 2.0 * 1.5 * C_BIT_PERIOD;
        assert t = 300 ns report "real product" severity failure;
        t := r * C_BIT_PERIOD;
        assert t = 250 ns report "real count" severity failure;

        -- And division, which counts the same way
        t := (3 * 13 * C_BIT_PERIOD) / 3;
        assert t = 1300 ns report "divided" severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
