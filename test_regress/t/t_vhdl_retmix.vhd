-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A result is as long as whichever return path ran.  Where one path has a
-- length that is a number and another has one only running measures, the two
-- settle nothing between them, so the result carries its bounds.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_retmix is
end entity;

architecture a of t_vhdl_retmix is

    function len_slv (v : std_logic_vector) return integer is
    begin
        return v'length;
    end function;

    function len_uns (v : unsigned) return integer is
    begin
        return v'length;
    end function;

    -- A length only running measures
    function pick (n : integer) return std_logic_vector is
    begin
        if n = 1 then
            return "1010";
        else
            return "11001100";
        end if;
    end function;

    -- One path is two elements, the other is whatever pick made: the two
    -- agree about nothing, so neither says how long the result is
    function mixed (a : unsigned; n : integer) return unsigned is
    begin
        if n = 0 then
            return "11";
        end if;
        return unsigned(pick(n));
    end function;

    -- The same without a conversion in the way
    function mixed_direct (n : integer) return std_logic_vector is
    begin
        if n = 0 then
            return "111";
        end if;
        return pick(n);
    end function;

    -- The unfolded path comes first and the folding one after, so the two
    -- have to disagree whichever order they are met in.  Here the first
    -- path's length is an expression rather than a number.
    function tail (v : std_logic_vector; n : integer) return std_logic_vector is
    begin
        if n > 0 then
            return v(v'left downto v'left - n + 1);
        end if;
        return "11";
    end function;

    -- Every path a number, and the same number: that is the length
    function agreed (n : integer) return std_logic_vector is
    begin
        if n = 0 then
            return "1100";
        end if;
        return "0011";
    end function;

    -- A path returning no elements at all says nothing about the paths that
    -- have some, and must go on saying nothing
    function guard_null (v : std_logic_vector) return std_logic_vector is
        constant C_NONE : std_logic_vector(0 downto 1) := (others => '0');
    begin
        if v'length < 1 then
            return C_NONE;
        end if;
        return v;
    end function;

begin

    p: process is
        variable a : unsigned(15 downto 0) := x"beef";
        variable v : std_logic_vector(4 downto 0) := "10110";
        variable z : integer := 0;
        variable o : integer := 1;
        variable t : integer := 2;
    begin
        assert len_uns(mixed(a, z)) = 2 report "mixed two" severity failure;
        assert len_uns(mixed(a, o)) = 4 report "mixed four" severity failure;
        assert len_uns(mixed(a, t)) = 8 report "mixed eight" severity failure;
        assert mixed(a, z) = "11" report "mixed two value" severity failure;
        assert mixed(a, o) = "1010" report "mixed four value" severity failure;
        assert mixed(a, t) = "11001100" report "mixed eight value" severity failure;

        assert len_slv(mixed_direct(z)) = 3 report "direct three" severity failure;
        assert len_slv(mixed_direct(o)) = 4 report "direct four" severity failure;
        assert len_slv(mixed_direct(t)) = 8 report "direct eight" severity failure;
        assert mixed_direct(z) = "111" report "direct three value" severity failure;
        assert mixed_direct(t) = "11001100" report "direct eight value" severity failure;

        assert len_slv(tail(v, z)) = 2 report "tail two" severity failure;
        assert len_slv(tail(v, t)) = 2 report "tail slice two" severity failure;
        assert tail(v, t) = "10" report "tail slice two value" severity failure;
        assert len_slv(tail(v, 4)) = 4 report "tail slice four" severity failure;
        assert tail(v, z) = "11" report "tail two value" severity failure;
        assert tail(v, 4) = "1011" report "tail slice four value" severity failure;

        -- What already worked, so a fix cannot be a coincidence
        assert len_slv(agreed(z)) = 4 report "agreed length" severity failure;
        assert agreed(z) = "1100" report "agreed value" severity failure;
        assert agreed(o) = "0011" report "agreed other value" severity failure;

        assert len_slv(guard_null(v)) = 5 report "guard_null length" severity failure;
        assert guard_null(v) = "10110" report "guard_null value" severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
