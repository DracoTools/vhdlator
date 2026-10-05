-- DESCRIPTION: Verilator: VHDL block with a generic type
--
-- "generic (type t); generic map (t => t_array)" gives a block's generic
-- type the type its generic map names (6.5.3, 11.2), and a port of that
-- type takes its subtype from its actual where the type is an unconstrained
-- array. INTEGER, BIT_VECTOR and an array of records are given (nvc
-- issue972).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_blkgentype is
end entity;

architecture test of t_vhdl_blkgentype is
    type t_rec is record
        f : natural;
    end record;
    type t_array is array (natural range <>) of t_rec;

    signal p, q : integer;
    signal x, y : bit_vector(3 downto 0);
    signal r, s : t_array(1 downto 0);
begin
    b1 : block is
        generic (type t);
        generic map (t => integer);
        port (i : in t; o : out t);
        port map (i => p, o => q);
    begin
        o <= i;
    end block;

    b2 : block is
        generic (type t);
        generic map (t => bit_vector);
        port (i : in t; o : out t);
        port map (i => x, o => y);
    begin
        o <= i;
    end block;

    b3 : block is
        generic (type t);
        generic map (t => t_array);
        port (i : in t; o : out t);
        port map (i => r, o => s);
    begin
        o <= i after 1 ns;
    end block;

    process is
    begin
        p <= 5;
        x <= "1010";
        r <= (0 => (f => 3), 1 => (f => 4));
        wait for 2 ns;
        assert q = 5 report "q" severity failure;
        assert y = "1010" report "y" severity failure;
        assert s(0).f = 3 and s(1).f = 4 report "s" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
