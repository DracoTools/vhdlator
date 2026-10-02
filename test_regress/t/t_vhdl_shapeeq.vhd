-- DESCRIPTION: Verilator: VHDL equality of arrays of different shapes
--
-- Two arrays are equal only where they have the same number of elements in
-- each dimension, and matching elements are equal (9.2.3).  Two shapes of
-- one array type are compared like any two values of it: unequal.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_shapeeq is
end entity;

architecture test of t_vhdl_shapeeq is
    type t_int2d is array (natural range <>, natural range <>) of integer;
    type t_int_int is array (natural range <>) of integer_vector;

    type t_rec is record
        x : integer;
        y : integer_vector;
    end record;

    type t_rec_array is array (natural range <>) of t_rec;

    function same (x, y : t_int2d) return boolean is
    begin
        return x = y;
    end function;

    function same (x, y : t_int_int) return boolean is
    begin
        return x = y;
    end function;

    function differ (x, y : t_int_int) return boolean is
    begin
        return x /= y;
    end function;

    function same (x, y : t_rec_array) return boolean is
    begin
        return x = y;
    end function;
begin

    process is
        variable a1 : t_int2d(1 to 3, 2 to 3) := (others => (others => 0));
        variable a2 : t_int2d(2 to 3, 1 to 3) := (others => (others => 0));
        variable a3 : t_int2d(0 to 2, 0 to 1) := (others => (others => 0));
        variable b1 : t_int_int(1 to 3)(2 to 3) := (others => (others => 0));
        variable b2 : t_int_int(2 to 3)(1 to 3) := (others => (others => 0));
        variable b3 : t_int_int(0 to 2)(0 to 1) := (others => (others => 0));
        variable b4 : t_int_int(0 to 2)(0 to 2) := (others => (others => 0));
        variable r1 : t_rec_array(1 to 2)(y(1 to 3)) := (others => (x => 0, y => (others => 0)));
        variable r2 : t_rec_array(1 to 2)(y(1 to 2)) := (others => (x => 0, y => (others => 0)));
        variable r3 : t_rec_array(3 to 4)(y(0 to 2)) := (others => (x => 0, y => (others => 0)));
    begin
        -- Another shape is another value
        assert not same(a1, a2) severity failure;
        assert not (a1 = a2) severity failure;
        assert a1 /= a2 severity failure;
        assert not same(b1, b2) severity failure;
        assert differ(b1, b2) severity failure;
        assert not same(b3, b4) severity failure;
        assert b3 /= b4 severity failure;
        assert not same(r1, r2) severity failure;
        assert r1 /= r2 severity failure;

        -- The same shape is compared element by element, whatever the bounds
        assert same(a1, a3) severity failure;
        assert same(b1, b3) severity failure;
        assert not differ(b1, b3) severity failure;
        assert same(r1, r3) severity failure;
        a3(1, 1) := 5;
        b3(1)(1) := 5;
        r3(4).y(2) := 5;
        assert not same(a1, a3) severity failure;
        assert not same(b1, b3) severity failure;
        assert differ(b1, b3) severity failure;
        assert not same(r1, r3) severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
