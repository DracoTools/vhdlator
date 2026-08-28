-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_specslice is
end entity;

architecture test of t_vhdl_specslice is

    -- The slice below is longer than the argument whenever "width" is, and
    -- the guard is what keeps it from ever being taken. Neither bound is
    -- static where this is written; both are settled by the call.
    function fit (s : string; width : natural) return string is
        variable v : string(1 to s'length) := s;
    begin
        if s'length >= width then
            return v(1 to width);
        end if;
        return v;
    end function;

begin
    check : process is
    begin
        -- width longer than the argument: the slice is never evaluated
        assert fit("abc", 8) = "abc" report "long: " & fit("abc", 8) severity failure;
        -- width shorter: the slice is taken and is within range
        assert fit("abcdef", 4) = "abcd" report "short: " & fit("abcdef", 4) severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
