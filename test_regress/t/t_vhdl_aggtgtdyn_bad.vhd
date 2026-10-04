-- DESCRIPTION: Verilator: VHDL aggregate target given a value of the wrong length
--
-- "(i1, i0) := x(4 downto lo)" with x(4 downto lo) four long once lo is 1:
-- the names on the left cover two elements, and lengths that do not match
-- are an error that ends the run (10.6.2.1), as nvc's bounds35 expects.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_aggtgtdyn_bad is
end entity;

architecture test of t_vhdl_aggtgtdyn_bad is
begin
    process is
        variable x : integer_vector(4 downto 0) := (1, 2, 3, 4, 5);
        variable i0, i1, lo : integer;
    begin
        lo := 3;
        (i1, i0) := x(4 downto lo);
        assert i1 = 1 and i0 = 2 severity failure;
        lo := 1;
        wait for 1 ns;
        (i1, i0) := x(4 downto lo);
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
