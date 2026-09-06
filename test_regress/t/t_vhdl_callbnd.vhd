-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- What a function gives back is as long as the value it returns, and starts
-- where that value starts.  A function with no parameters has nothing else
-- to say what its result's bounds are, so they have to come off its body.

entity t_vhdl_callbnd is
end entity;

architecture a of t_vhdl_callbnd is

    function get_left (s : string) return integer is
    begin
        return s'left;
    end function;

    function get_right (s : string) return integer is
    begin
        return s'right;
    end function;

    function get_length (s : string) return integer is
    begin
        return s'length;
    end function;

    -- No parameters at all: only the body says what the result is
    function mk return string is
        variable s : string(50 to 55) := "abcdef";
    begin
        return s;
    end function;

    -- A parameter, but a constrained one, so it fixes nothing either
    function mk_of (c : character) return string is
        variable s : string(20 downto 16) := "wxyz" & c;
    begin
        return s;
    end function;

    -- Through a second call, which is where this was first noticed
    function change_bounds (s : string; l, r : positive) return string is
        alias ss : string(l to r) is s;
    begin
        return ss;
    end function;

    function relabel (c : character) return string is
        variable s : string(1 to 5) := "abcd" & c;
    begin
        return change_bounds(s, 101, 105);
    end function;

begin

    p: process is
        variable v : string(50 to 55) := "abcdef";
    begin
        -- What already worked, so a fix cannot be a coincidence
        assert get_left(v) = 50 report "direct left" severity failure;
        assert v'left = 50 report "own left" severity failure;

        assert get_left(mk) = 50 report "mk left" severity failure;
        assert get_right(mk) = 55 report "mk right" severity failure;
        assert get_length(mk) = 6 report "mk length" severity failure;
        assert mk = "abcdef" report "mk value" severity failure;

        assert get_left(mk_of('z')) = 20 report "mk_of left" severity failure;
        assert get_right(mk_of('z')) = 16 report "mk_of right" severity failure;
        assert mk_of('z') = "wxyzz" report "mk_of value" severity failure;

        assert get_left(relabel('e')) = 101 report "relabel left" severity failure;
        assert get_right(relabel('e')) = 105 report "relabel right" severity failure;
        assert relabel('e') = "abcde" report "relabel value" severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
