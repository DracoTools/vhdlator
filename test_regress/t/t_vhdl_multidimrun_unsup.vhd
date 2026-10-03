-- DESCRIPTION: Verilator: VHDL array of several dimensions whose bounds
-- only running decides, used other than an element at a time
--
-- Such an array is held as one run of its elements, read and written an
-- element at a time. Copying the whole of it, and declaring one with an
-- initial value other than one value for every element, are refused where
-- they are written.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_multidimrun_unsup is
end entity;

architecture test of t_vhdl_multidimrun_unsup is
    type int2d is array (natural range <>, natural range <>) of integer;

    function corner (x, y : integer) return integer is
        variable r, s : int2d(1 to x, 1 to y);
        variable z : int2d(1 to x, 1 to y) := (1 => (others => 0), others => (others => 1));
    begin
        for i in 1 to x loop
            for j in 1 to y loop
                r(i, j) := i + j;
            end loop;
        end loop;
        s := r;
        return s(x, y) + z(1, 1);
    end function;
begin
    process is
        variable x, y : integer;
    begin
        x := 4;
        y := 5;
        assert corner(x, y) = 9 severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
