-- DESCRIPTION: Verilator: VHDL aggregate target given a value whose bounds only running decides
--
-- "(i1, i0) := x(4 downto lo)" with lo a variable: the value carries its
-- own bounds, and its elements go to the names on the left in order
-- (10.6.2.1). It was lowered as an array of fixed length, which the back
-- end refused (nvc bounds35, bounds36, bounds37).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_aggtgtdyn is
end entity;

architecture test of t_vhdl_aggtgtdyn is
    signal sx : integer_vector(4 downto 0) := (1, 2, 3, 4, 5);
    signal s1 : integer;
    signal sy : integer_vector(1 downto 0);
    signal sb : bit_vector(0 to 2);
begin
    process is
        variable x : integer_vector(4 downto 0);
        variable u : integer_vector(0 to 4);
        variable y : integer_vector(1 downto 0);
        variable b : bit_vector(0 to 5) := "101100";
        variable i0, i1, lo : integer;
        variable c0, c1 : bit;
        variable rest : bit_vector(0 to 1);
    begin
        x := (1, 2, 3, 4, 5);
        u := (10, 20, 30, 40, 50);
        lo := 3;
        (i1, i0) := x(4 downto lo);
        assert i1 = 1 and i0 = 2 severity failure;
        lo := 2;
        (i1, y) := x(4 downto lo);
        assert i1 = 1 and y = (2, 3) severity failure;
        (y, i0) := u(lo to 4);
        assert y = (30, 40) and i0 = 50 severity failure;
        (c0, c1, rest) := b(lo to lo + 3);
        assert c0 = '1' and c1 = '1' and rest = "00" severity failure;
        lo := 3;
        (s1, sy) <= sx(4 downto lo - 1);
        sb <= b(lo - 1 to lo + 1);
        wait for 1 ns;
        assert s1 = 1 and sy = (2, 3) severity failure;
        assert sb = "110" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
