-- DESCRIPTION: Verilator: VHDL constant of a generate's block bounding a slice
--
-- A constant declared in a generate's block is elaborated with the block
-- (11.8): one whose value an open port's bounds give is as settled as they
-- are, so a slice written in terms of it and the generate's index has a
-- width each instance fixes. The bounds of such a slice are the range it
-- was written with (8.5), not those of the port.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_genconstslice_sub is
    port ( i : in bit_vector; o : out bit_vector );
end entity;

architecture test of t_vhdl_genconstslice_sub is
begin
    -- Each half of the input, reversed in place
    process (i) is
    begin
        assert i(i'low + 1 to i'low + 2)'length = 2 severity failure;
        assert i(i'low + 1 to i'low + 2)'left = i'low + 1 severity failure;
        assert i(i'low + 1 to i'low + 2)'right = i'low + 2 severity failure;
        assert i(i'low + 1 to i'low + 2)'low = i'low + 1 severity failure;
        assert i(i'low + 1 to i'low + 2)'high = i'low + 2 severity failure;
        assert i(i'low + 1 to i'low)'length = 0 severity failure;
    end process;

    g: for n in 0 to 1 generate
        constant stride : natural := i'length / 2;
        constant base : natural := i'low + n * stride;
    begin
        swap: for k in 0 to stride - 1 generate
            o(base + k) <= i(base + stride - 1 - k);
        end generate;
        check: process (i) is
        begin
            assert o(base to base + stride - 1)'length = stride severity failure;
        end process;
    end generate;
end architecture;

entity t_vhdl_genconstslice is
end entity;

architecture test of t_vhdl_genconstslice is
    signal a, b : bit_vector(0 to 7) := "10000011";
    signal c, d : bit_vector(4 to 7) := "1000";
begin
    u1: entity work.t_vhdl_genconstslice_sub port map ( a, b );
    u2: entity work.t_vhdl_genconstslice_sub port map ( c, d );

    process is
    begin
        wait for 1 ns;
        assert b = "00011100" severity failure;
        assert d = "0100" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
