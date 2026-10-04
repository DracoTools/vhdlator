-- DESCRIPTION: Verilator: VHDL concatenation of elements and aggregates of
-- an array whose elements do not pack
--
-- "a & b & (0 => 4)" of INTEGERs is an INTEGER_VECTOR of three: each
-- INTEGER operand is one element and the aggregate is one more (9.2.5). The
-- aggregate's named choice is no index into the result, only into the
-- aggregate itself, which runs the way its index subtype does (nvc simp
-- ports2008).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_catelemagg_sub is
    port (y : in integer_vector(1 to 3); s1, s2, s3 : out integer);
end entity;

architecture a of t_vhdl_catelemagg_sub is
begin
    s1 <= y(1);
    s2 <= y(2);
    s3 <= y(3);
end architecture;

entity t_vhdl_catelemagg is
end entity;

architecture test of t_vhdl_catelemagg is
    signal a : integer := 1;
    signal b : integer := 2;
    signal y3 : integer_vector(1 to 3);
    signal y4 : integer_vector(0 to 3);
    signal s1, s2, s3 : integer;
begin
    u : entity work.t_vhdl_catelemagg_sub port map (y => a & b & (0 => 4), s1 => s1, s2 => s2, s3 => s3);

    process is
        variable v : integer_vector(1 to 5);
    begin
        y3 <= a & (4, 5);
        y4 <= (7 => 9, 8 => 8) & a & b;
        v := (2 => 6, 1 => 5, 3 => 7) & a & b;
        wait for 1 ns;
        assert s1 = 1 and s2 = 2 and s3 = 4 report "port" severity failure;
        assert y3 = (1, 4, 5) report "y3" severity failure;
        assert y4 = (9, 8, 1, 2) report "y4" severity failure;
        assert v = (5, 6, 7, 1, 2) report "v" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
