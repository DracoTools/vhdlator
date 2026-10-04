-- DESCRIPTION: Verilator: VHDL block generic of an unconstrained array type
--
-- "generic (taps : bit_vector)" takes the subtype of the value its generic
-- map gives it (6.5.6.2), so "taps'range" is a range inside the block
-- (GHDL gna issue2445).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_blkgenunc is
end entity;

architecture test of t_vhdl_blkgenunc is
    constant c : bit_vector := x"d";
    constant d : bit_vector(7 downto 2) := "110010";
begin
    b1 : block
        generic (taps : bit_vector);
        generic map (taps => c);
        subtype lfsr_t is bit_vector(taps'range);
        signal s : lfsr_t := taps;
    begin
        process is
        begin
            assert lfsr_t'left = 0 and lfsr_t'right = 3 severity failure;
            assert s = "1101" severity failure;
            assert taps'length = 4 severity failure;
            wait;
        end process;
    end block;

    b2 : block
        generic (g : bit_vector; n : natural);
        generic map (d, 5);
    begin
        process is
        begin
            wait for 1 ns;
            assert g'left = 7 and g'right = 2 and not g'ascending severity failure;
            assert g(7) = '1' and g(2) = '0' and n = 5 severity failure;
            report "*-* All Finished *-*";
            std.env.finish;
        end process;
    end block;
end architecture;
