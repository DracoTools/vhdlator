-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- The relational operators over a one-dimensional array of a discrete type are
-- lexicographic: the first pair of elements that differ decides, and where one
-- operand runs out first the shorter of the two is the lesser (5.2.6).
--
-- An array of bits lowers to a packed vector, whose plain unsigned comparison
-- gives the same answer. One of integers lowers to an unpacked array, which
-- has no comparison of its own -- so the emitted C++ said "no match for
-- operator>" and did not build, while --lint-only reported nothing.
--
-- Only a discrete element type has these operators at all, so an array of REAL
-- has none and is rightly refused.

entity t_vhdl_arraycmp is
end entity;

architecture a of t_vhdl_arraycmp is
begin

    main: process is
        variable a : integer_vector (1 to 3) := (4, 9, 2);
        variable c : integer_vector (1 to 3) := (5, 0, 0);
        variable e : integer_vector (1 to 3) := (4, 9, 2);
        variable d : integer_vector (1 to 5) := (4, 9, 2, 1, 1);
        variable f : integer_vector (1 to 2) := (4, 9);
        variable g : integer_vector (3 downto 1) := (4, 9, 2);
    begin
        -- The first difference decides, not the last
        assert not (a > c) report "a > c" severity failure;
        assert a < c report "a < c" severity failure;

        -- Equal as far as the shorter goes, so the shorter is the lesser
        assert a < d report "a < d" severity failure;
        assert d > a report "d > a" severity failure;
        assert f < a report "f < a" severity failure;

        -- Two equal arrays are neither greater nor less, but are both
        assert a >= e report "a >= e" severity failure;
        assert a <= e report "a <= e" severity failure;
        assert not (a > e) report "a > e" severity failure;
        assert not (a < e) report "a < e" severity failure;

        -- The comparison walks the values from their leftmost element,
        -- whichever way the index range runs
        assert not (a < g) and not (a > g) report "a vs g" severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
        wait;
    end process;

end architecture;
