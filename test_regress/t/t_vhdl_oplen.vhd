-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Of two operands the result is the longer (16.5), and an operand that says
-- no length of its own may still be one that can be measured: a
-- concatenation is as long as its parts together (9.2.5). Taking the other
-- operand's length instead leaves the result short by the part that went
-- uncounted, which is how ieee.float_generic_pkg writes the exponent of
-- every sum it forms.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_oplen is
end entity;

architecture a of t_vhdl_oplen is
begin

    p: process is
        variable el, er : signed(7 downto 0)   := "00000011";
        variable ul, ur : unsigned(7 downto 0) := "00000011";
        variable t9     : signed(8 downto 0);
        variable sx     : signed(8 downto 0);
        variable ux     : unsigned(8 downto 0);
        variable lx     : signed(8 downto 0);
    begin
        -- ieee.float_generic_pkg-body.vhdl writes its exponent this way
        sx := (el(7) & el) - er;
        assert sx'length = 9 report "concat sub length" severity failure;
        assert to_integer(sx) = 0 report "concat sub value" severity failure;

        sx := (el(7) & el) + er;
        assert to_integer(sx) = 6 report "concat add value" severity failure;

        -- with the concatenation on the right instead
        sx := er - (el(7) & el);
        assert sx'length = 9 report "rev concat sub length" severity failure;
        assert to_integer(sx) = 0 report "rev concat sub value" severity failure;

        -- a slice concatenated, not an element
        sx := (el(7 downto 7) & el) - er;
        assert to_integer(sx) = 0 report "slice concat value" severity failure;

        -- unsigned answers the same way
        ux := (ul(7) & ul) - ur;
        assert ux'length = 9 report "unsigned concat length" severity failure;
        assert to_integer(ux) = 0 report "unsigned concat value" severity failure;

        -- a logical operator is as long as its operands too
        lx := (el(7) & el) and (er(7) & er);
        assert lx'length = 9 report "concat and length" severity failure;

        -- What already worked, so a fix cannot be a coincidence
        sx := el(7) & el;
        assert sx'length = 9 report "concat alone" severity failure;
        t9 := el(7) & el;
        sx := t9 - er;
        assert to_integer(sx) = 0 report "through a named object" severity failure;

        report "*-* All Finished *-*";
        wait;
    end process;

end architecture;
