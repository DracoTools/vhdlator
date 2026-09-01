-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

use std.textio.all;

entity t_vhdl_justify is
end entity;

architecture test of t_vhdl_justify is

    -- The width a report pads a column out to is often counted from the
    -- design's own constants, so it is not a number the text settles (16.4)
    function padded (s : string; n : integer) return string is
    begin
        return "[" & justify(s, RIGHT, n) & "]";
    end function;

begin

    main : process is
        variable n : integer := 6;
    begin
        assert justify("ab", RIGHT, n) = "    ab" report "right" severity failure;
        assert justify("ab", LEFT, n + 1) = "ab     " report "left" severity failure;
        -- a value already at or over the width comes back as it is
        assert justify("abcdef", RIGHT, n - 4) = "abcdef" report "over" severity failure;
        assert justify("abcdef", LEFT, n) = "abcdef" report "exact" severity failure;
        assert justify("", RIGHT, n)'length = 6 report "empty" severity failure;
        -- a width the text does settle still folds into the format
        assert justify("cd", RIGHT, 5) = "   cd" report "static" severity failure;
        assert padded("x", n / 2) = "[  x]" report "in a function" severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
