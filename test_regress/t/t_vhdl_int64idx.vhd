-- DESCRIPTION: Verilator: VHDL array indexed by a 64-bit integer type, bounds beyond 32 bits
--
-- An integer type may have any range a 64-bit integer holds (5.2.3), and an
-- array indexed by it may start at its most negative value.  Its elements
-- are reached by those indices, and its attributes say them.  nvc and GHDL
-- run this; Questa 2025.2 takes no integer wider than 32 bits.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_int64idx is
end entity;

architecture test of t_vhdl_int64idx is
    type int64_t is range -9223372036854775807 - 1 to 9223372036854775807;
    type array_t is array (int64_t range <>) of int64_t;

    function get_length (a : array_t) return integer is
    begin
        return a'length;
    end function;

begin

    p1 : process is
        variable x, y : int64_t;
    begin
        assert x = -9223372036854775807 - 1 severity failure;
        x := 1;
        y := 5;
        wait for 1 ns;
        assert x + y = 6 severity failure;
        x := int64_t(integer'high) + 1;
        assert int64_t'image(x) = "2147483648" severity failure;
        assert int64_t'value("-9223372036854775807") = -9223372036854775807 severity failure;
        wait;
    end process;

    p2 : process is
        variable a : array_t(-9223372036854775807 to -9223372036854775800);
        variable b : array_t(5000000000 downto 4999999998);
    begin
        assert a'length = 8 severity failure;
        assert get_length(a) = 8 severity failure;
        a(-9223372036854775801) := 77;
        assert a(-9223372036854775801) = 77 severity failure;
        assert a(-9223372036854775800) = int64_t'left severity failure;
        b(5000000000) := 1;
        b(4999999998) := 3;
        assert b(5000000000) + b(4999999998) = 4 severity failure;
        assert b'left = 5000000000 and b'right = 4999999998 severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
