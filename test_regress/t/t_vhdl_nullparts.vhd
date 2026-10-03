-- DESCRIPTION: Verilator: VHDL null arrays as parts of an aggregate target, and as slices returned
--
-- A name of no elements in an aggregate target takes nothing of the value
-- (10.6.2.1); one of a single element is still a run of the array type.
-- A slice whose range is null is an array of no elements (5.2.1), and two
-- arrays of no elements are equal (9.2.3).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_nullparts is
end entity;

architecture test of t_vhdl_nullparts is

    function get_ints (count : natural) return integer_vector is
        variable result : integer_vector(1 to count);
    begin
        for i in 1 to count loop
            result(i) := i;
        end loop;
        return result;
    end function;

    procedure proc (xl, yl : natural) is
        variable x : integer_vector(1 to xl);
        variable y : integer_vector(1 to yl);
    begin
        (x, y) := get_ints(5);
        for i in 1 to xl loop
            assert x(i) = i severity failure;
        end loop;
        for i in 1 to yl loop
            assert y(i) = xl + i severity failure;
        end loop;
    end procedure;

    type int_array is array (natural range <>) of integer_vector;

    function sum_all (x : int_array) return integer is
        variable result : integer := 0;
    begin
        for i in x'range loop
            for j in x(i)'range loop
                result := result + x(i)(j);
            end loop;
        end loop;
        return result;
    end function;

    function get_slice (x : int_array; l, r : natural) return int_array is
    begin
        return x(l to r);
    end function;

    signal s1 : int_array(1 to 3)(1 to 2) := ((1, 2), (3, 4), (5, 6));
begin

    p1 : process is
    begin
        proc(2, 3);
        proc(1, 4);
        proc(0, 5);
        proc(5, 0);
        proc(4, 1);

        assert sum_all(s1) = 21 severity failure;
        assert get_slice(s1, 1, 2) = ((1, 2), (3, 4)) severity failure;
        assert get_slice(s1, 3, 3) = (0 => (5, 6)) severity failure;
        assert get_slice(s1, 3, 0) = (1 to 0 => (1, 1)) severity failure;
        assert get_slice(s1, 1, 2)(2) = (3, 4) severity failure;
        s1(2)(2) <= 10;
        wait for 1 ns;
        assert sum_all(s1) = 27 severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
