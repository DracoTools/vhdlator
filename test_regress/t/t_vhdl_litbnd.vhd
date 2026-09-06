-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A string literal has no bounds of its own, so it takes them from the index
-- subtype of the array type it belongs to (5.3.2.2, 9.3.2): 0 upwards for a
-- STD_LOGIC_VECTOR, whose index is NATURAL, and 1 upwards for a STRING, whose
-- index is POSITIVE.  That holds wherever the literal ends up, including in a
-- result whose length only running measures.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_litbnd is
end entity;

architecture a of t_vhdl_litbnd is

    function left_of (v : std_logic_vector) return integer is
    begin
        return v'left;
    end function;

    function right_of (v : std_logic_vector) return integer is
    begin
        return v'right;
    end function;

    -- A result whose length only running measures, returning literals
    function pick (n : integer) return std_logic_vector is
    begin
        if n = 1 then
            return "1010";
        else
            return "11001100";
        end if;
    end function;

    -- The same, returning objects with bounds written on them
    function pick_obj (n : integer) return std_logic_vector is
        variable a : std_logic_vector(20 downto 17) := "1010";
        variable b : std_logic_vector(30 downto 23) := "11001100";
    begin
        if n = 1 then
            return a;
        else
            return b;
        end if;
    end function;

    function mk_str (n : integer) return string is
    begin
        if n = 1 then
            return "ab";
        else
            return "cdef";
        end if;
    end function;

begin

    p: process is
        variable o : integer := 1;
        variable t : integer := 2;
        variable s : string(1 to 4);
    begin
        -- What a literal is worth where nothing carries it, so a fix here
        -- cannot be a coincidence
        assert pick(o)'length = 4 report "pick short length" severity failure;
        assert pick(t)'length = 8 report "pick long length" severity failure;

        assert pick(o)'left = 0 report "pick short left" severity failure;
        assert pick(o)'right = 3 report "pick short right" severity failure;
        assert pick(t)'left = 0 report "pick long left" severity failure;
        assert pick(t)'right = 7 report "pick long right" severity failure;
        assert pick(o) = "1010" report "pick short value" severity failure;
        assert pick(t) = "11001100" report "pick long value" severity failure;

        -- Bounds the source wrote, which already arrived intact
        assert left_of(pick_obj(o)) = 20 report "obj short left" severity failure;
        assert right_of(pick_obj(o)) = 17 report "obj short right" severity failure;
        assert left_of(pick_obj(t)) = 30 report "obj long left" severity failure;
        assert right_of(pick_obj(t)) = 23 report "obj long right" severity failure;

        -- A STRING is indexed from 1, not from 0
        assert mk_str(o)'left = 1 report "str short left" severity failure;
        assert mk_str(o)'right = 2 report "str short right" severity failure;
        assert mk_str(t)'left = 1 report "str long left" severity failure;
        assert mk_str(t)'right = 4 report "str long right" severity failure;
        s := mk_str(t);
        assert s = "cdef" report "str value" severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
