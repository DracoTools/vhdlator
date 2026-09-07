-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- TO_01, TO_X01 and the conversions of std_logic_1164 hand back what they
-- were given, element for element, so the result is exactly as long as the
-- argument. The frontend carries these out itself rather than compiling
-- their bodies, so nothing else measures the result -- and a value of no
-- length cannot be passed to a subprogram written against an unconstrained
-- array, because the copy made for that call site is what gives the
-- parameter a width.
--
-- ieee.float_generic_pkg does exactly that: "find_leftmost (to_01(fract),
-- '1')" is how it normalizes every arithmetic result.
--
-- How the result is numbered is each declaration's own business, and the two
-- families in the IEEE bodies do not agree: TO_01 and the conversions
-- between vector types count down from the length less one, while TO_X01 and
-- TO_UX01 count up from one. The numbers below are what the bodies shipped
-- in include/vhdl/ieee say, and GHDL 7.0.0-dev agrees with every one of
-- them. nvc 1.22.1 differs on TO_X01 over a vector alone, where it answers
-- "length-1 downto 0" although the body it ships says "1 to length".

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_convbnd is
end entity;

architecture a of t_vhdl_convbnd is

    -- Written against unconstrained arrays, as numeric_std's own
    -- FIND_LEFTMOST is, so each call has to say how long its actual is --
    -- and reading the bounds through one of these is also the only portable
    -- way to ask, since the prefix of an array attribute must be an object
    -- and a function call is not one (16.2)
    function lft (s : std_ulogic_vector) return integer is
    begin
        return s'left;
    end function;

    function rgt (s : std_ulogic_vector) return integer is
    begin
        return s'right;
    end function;

    function ones (s : std_ulogic_vector) return natural is
        variable n : natural := 0;
    begin
        for i in s'range loop
            if s (i) = '1' then
                n := n + 1;
            end if;
        end loop;
        return n;
    end function;

    function highest_set (arg : unsigned) return integer is
    begin
        for i in arg'high downto arg'low loop
            if arg (i) = '1' then
                return i;
            end if;
        end loop;
        return -1;
    end function;

    function bit_lft (s : bit_vector) return integer is
    begin
        return s'left;
    end function;

    function bit_rgt (s : bit_vector) return integer is
    begin
        return s'right;
    end function;

begin

    main: process is
        variable u  : unsigned (11 downto 4) := "00110100";
        variable sv : std_logic_vector (2 to 9) := "01010001";
        variable bv : bit_vector (7 downto 0) := "11000001";
    begin
        -- TO_01 counts down from the length less one, so an index means
        -- there what the body says and not what the argument said
        assert lft (std_ulogic_vector (to_01 (u))) = 7
            report "bad to_01 left" severity failure;
        assert rgt (std_ulogic_vector (to_01 (u))) = 0
            report "bad to_01 right" severity failure;
        assert highest_set (to_01 (u)) = 5 report "bad highest_set" severity failure;

        -- TO_X01 and TO_UX01 count up from one
        assert lft (to_x01 (sv)) = 1 report "bad to_x01 left" severity failure;
        assert rgt (to_x01 (sv)) = 8 report "bad to_x01 right" severity failure;
        assert ones (to_x01 (sv)) = 3 report "bad count to_x01" severity failure;
        assert lft (to_ux01 (sv)) = 1 report "bad to_ux01 left" severity failure;

        assert lft (to_stdulogicvector (bv)) = 7
            report "bad to_stdulogicvector left" severity failure;
        assert ones (to_stdulogicvector (bv)) = 3
            report "bad count to_stdulogicvector" severity failure;

        assert bit_lft (to_bitvector (sv)) = 7
            report "bad to_bitvector left" severity failure;
        assert bit_rgt (to_bitvector (sv)) = 0
            report "bad to_bitvector right" severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
        wait;
    end process;

end architecture;
