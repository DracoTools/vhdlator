-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A qualified expression is as long as what it qualifies where the type mark
-- names an unconstrained array (5.3.2.2, 9.3.5), so "integer_vector'(7, 8, 9)"
-- is three elements even though INTEGER_VECTOR itself says no length.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_qualbnd is
end entity;

architecture a of t_vhdl_qualbnd is

    function len_iv (v : integer_vector) return integer is
    begin
        return v'length;
    end function;

    -- A result whose length only running decides
    function decode (n : integer) return integer_vector is
    begin
        if n = 1 then
            return (7, 8, 9);
        else
            return (1, 2);
        end if;
    end function;

    function decode_slv (n : integer) return std_logic_vector is
    begin
        if n = 1 then
            return "1010";
        else
            return "11001100";
        end if;
    end function;

    constant K : integer_vector := (7, 8, 9);

begin

    p: process is
        variable o : integer := 1;
        variable t : integer := 2;
        variable v : integer_vector(0 to 2) := (7, 8, 9);
    begin
        -- A qualified aggregate says how long it is
        assert len_iv(integer_vector'(7, 8, 9)) = 3 report "qual length" severity failure;
        assert len_iv(integer_vector'(0 => -42)) = 1 report "qual one" severity failure;

        -- Compared with a value whose length only running decides
        assert decode(o) = integer_vector'(7, 8, 9) report "qual eq" severity failure;
        assert decode(t) = integer_vector'(1, 2) report "qual eq two" severity failure;
        assert decode(o) /= integer_vector'(1, 2) report "qual ne" severity failure;
        assert decode(t) /= integer_vector'(7, 8, 9) report "qual ne two" severity failure;

        assert decode_slv(o) = std_logic_vector'("1010") report "qual slv eq" severity failure;
        assert decode_slv(t) /= std_logic_vector'("1010") report "qual slv ne" severity failure;

        -- What already worked, so a fix cannot be a coincidence
        assert decode(o) = K report "against a constant" severity failure;
        assert decode(o) = v report "against a variable" severity failure;
        assert decode(o) = (7, 8, 9) report "unqualified aggregate" severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
