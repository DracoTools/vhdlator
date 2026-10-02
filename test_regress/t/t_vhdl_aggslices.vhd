-- DESCRIPTION: Verilator: VHDL aggregate of an array of arrays whose elements are slices
--
-- Each element of the aggregate is a slice of another array, which is a
-- value of the element type like any other.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_aggslices is
end entity;

architecture test of t_vhdl_aggslices is
    type int_vec is array (natural range <>) of integer;
    type int_vec4x2 is array (1 to 4) of int_vec(1 to 2);
    type int_vec2x3 is array (0 to 1) of int_vec(2 downto 0);
begin

    process is
        variable a : int_vec4x2;
        variable b : int_vec(1 to 8);
        variable c : int_vec2x3;
    begin
        b := (1, 2, 3, 4, 5, 6, 7, 8);
        a := int_vec4x2'(b(1 to 2), b(3 to 4), b(5 to 6), b(7 to 8));
        assert a(1) = (1, 2) severity failure;
        assert a(2) = (3, 4) severity failure;
        assert a(3) = (5, 6) severity failure;
        assert a(4) = (7, 8) severity failure;
        a := (4 => b(1 to 2), others => b(7 to 8));
        assert a(4) = (1, 2) and a(1) = (7, 8) severity failure;
        -- Elements numbered the other way round from the slice
        c := (b(2 to 4), b(6 to 8));
        assert c(0)(2) = 2 and c(0)(0) = 4 severity failure;
        assert c(1)(2) = 6 and c(1)(0) = 8 severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
