-- DESCRIPTION: Verilator: VHDL arrays of several dimensions whose bounds
-- only running decides
--
-- A function's local array of three dimensions sized by its parameters
-- (NVC func10), one of two dimensions with a descending dimension, and a
-- procedure's local, one given a value for every element: each
-- element read and written by its indices, and each dimension's attributes
-- and range read with the dimension number (16.2.3).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_multidimrun is
end entity;

architecture test of t_vhdl_multidimrun is
    type int3d is array (natural range <>, natural range <>, natural range <>) of integer;
    type bool2d is array (integer range <>, integer range <>) of boolean;

    function middle (x, y, z : integer) return integer is
        variable r : int3d(1 to x, 1 to y, 1 to z);
        -- One value for every element, worked out once
        variable u : int3d(1 to x, 1 to y, 1 to z) := (others => (others => (others => x + y)));
    begin
        assert u(1, 1, 1) = 9 and u(x, y, z) = 9 severity failure;
        u(2, 3, 4) := 0;
        assert u(2, 3, 4) = 0 and u(2, 3, 5) = 9 severity failure;
        for i in 1 to x loop
            for j in 1 to y loop
                for k in 1 to z loop
                    r(i, j, k) := i * 100 + j * 10 + k;
                end loop;
            end loop;
        end loop;
        assert r'length(1) = x and r'length(2) = y and r'length(3) = z severity failure;
        return r(x / 2, y / 2, z / 2);
    end function;

    -- Counts the true elements of a grid whose second dimension runs down
    function count (lo, hi : integer) return integer is
        variable g : bool2d(lo to hi, hi downto lo);
        variable n : integer := 0;
    begin
        assert g'left(1) = lo and g'right(1) = hi and g'ascending(1) severity failure;
        assert g'left(2) = hi and g'right(2) = lo and not g'ascending(2) severity failure;
        assert g'low(2) = lo and g'high(2) = hi severity failure;
        for i in g'range(1) loop
            for j in g'reverse_range(2) loop
                g(i, j) := (i + j) mod 3 = 0;
            end loop;
        end loop;
        for i in g'range(1) loop
            for j in g'range(2) loop
                if g(i, j) then
                    n := n + 1;
                end if;
            end loop;
        end loop;
        return n;
    end function;

    procedure fill (n : integer; total : out integer) is
        variable t : int3d(0 to n - 1, 0 to 1, 2 downto 0);
    begin
        -- Every element starts at its subtype's left
        assert t(n - 1, 1, 0) = integer'left severity failure;
        total := 0;
        for i in t'range(1) loop
            for j in t'range(2) loop
                for k in t'range(3) loop
                    t(i, j, k) := i + j + k;
                end loop;
            end loop;
        end loop;
        for i in t'range(1) loop
            for j in t'range(2) loop
                for k in t'range(3) loop
                    total := total + t(i, j, k);
                end loop;
            end loop;
        end loop;
    end procedure;
begin
    process is
        variable x, y, z, total : integer;
    begin
        x := 4;
        y := 5;
        z := 6;
        wait for 1 ns;
        assert middle(x, y, z) = 223 severity failure;
        -- (i + j) mod 3 = 0 over 2..5 by 2..5: (2,4) (3,3) (4,2) (4,5) (5,4)
        assert count(2, 5) = 5 severity failure;
        fill(3, total);
        -- i + j + k over i in 0 to 2, j in 0 to 1, k in 0 to 2: 18 + 9 + 18
        assert total = 45 severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
