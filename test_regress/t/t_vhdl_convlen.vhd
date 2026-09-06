-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A type conversion is exactly as long as what it is written on, so where
-- that is a call whose result only running measures, the conversion is that
-- long too -- and no parameter of the enclosing function says otherwise.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_convlen is
end entity;

architecture a of t_vhdl_convlen is

    function len_slv (v : std_logic_vector) return integer is
    begin
        return v'length;
    end function;

    function len_uns (v : unsigned) return integer is
    begin
        return v'length;
    end function;

    -- A result whose length only running decides: two paths, two lengths
    function pick (n : integer) return std_logic_vector is
    begin
        if n = 1 then
            return "1010";
        else
            return "11001100";
        end if;
    end function;

    -- The conversion is as long as the call, not as long as "a"
    function conv_of_call (a : unsigned; n : integer) return unsigned is
    begin
        return unsigned(pick(n));
    end function;

    -- Two conversions written one inside the other, still the call's length
    function conv_twice (a : unsigned; n : integer) return std_logic_vector is
    begin
        return std_logic_vector(unsigned(pick(n)));
    end function;

    -- A conversion of a call whose length does fold keeps folding: "a" is
    -- what says how long this one is, and it must go on saying so
    function widen (v : std_logic_vector) return std_logic_vector is
        variable r : std_logic_vector(v'length - 1 downto 0);
    begin
        r := v;
        return r;
    end function;

    function conv_of_folded (a : unsigned) return unsigned is
    begin
        return unsigned(widen(std_logic_vector(a)));
    end function;

    -- A conversion of something that is not a call is as long as its
    -- operands, which is what a parameter of the same type says
    function conv_of_op (a : unsigned; b : unsigned) return unsigned is
    begin
        return unsigned(std_logic_vector(a) and std_logic_vector(b));
    end function;

begin

    p: process is
        variable a  : unsigned(15 downto 0) := x"beef";
        variable b  : unsigned(15 downto 0) := x"00ff";
        variable one : integer := 1;
        variable two : integer := 2;
    begin
        -- What already worked, so a fix cannot be a coincidence
        assert len_slv(pick(one)) = 4 report "pick short length" severity failure;
        assert len_slv(pick(two)) = 8 report "pick long length" severity failure;
        assert pick(one) = "1010" report "pick short value" severity failure;
        assert pick(two) = "11001100" report "pick long value" severity failure;

        assert len_uns(conv_of_call(a, one)) = 4 report "conv short length" severity failure;
        assert len_uns(conv_of_call(a, two)) = 8 report "conv long length" severity failure;
        assert conv_of_call(a, one) = "1010" report "conv short value" severity failure;
        assert conv_of_call(a, two) = "11001100" report "conv long value" severity failure;

        assert len_slv(conv_twice(a, one)) = 4 report "twice short length" severity failure;
        assert len_slv(conv_twice(a, two)) = 8 report "twice long length" severity failure;
        assert conv_twice(a, one) = "1010" report "twice short value" severity failure;
        assert conv_twice(a, two) = "11001100" report "twice long value" severity failure;

        assert len_uns(conv_of_folded(a)) = 16 report "folded length" severity failure;
        assert conv_of_folded(a) = x"beef" report "folded value" severity failure;

        assert len_uns(conv_of_op(a, b)) = 16 report "op length" severity failure;
        assert conv_of_op(a, b) = x"00ef" report "op value" severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
