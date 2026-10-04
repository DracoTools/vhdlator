-- DESCRIPTION: Verilator: VHDL alias of a slice whose bounds a function gives
--
-- "alias b is s(s'high downto log2_ceil(7))", with s sized by the same
-- function: the function is built on MATH_REAL, which analysis does not
-- fold, so neither s's bounds nor the slice's are numbers there. They are
-- fixed when the design is elaborated, and the alias's attributes are read
-- from the range as written (8.5; GHDL gna issue2540).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.math_real.all;

entity t_vhdl_aliasslicefn is
end entity;

architecture test of t_vhdl_aliasslicefn is
    function log2_ceil (val : natural) return natural is
    begin
        return integer(ceil(log2(real(val) + 1.0)));
    end function;

    constant base : natural := 16;
    signal cnt : unsigned(base + log2_ceil(7) - 1 downto 0);
    alias byte_cnt is cnt(cnt'high downto log2_ceil(7));
    alias low_part is cnt(log2_ceil(7) - 1 downto 0);
begin
    process is
    begin
        assert cnt'length = 19 severity failure;
        assert byte_cnt'length = 16 severity failure;
        assert byte_cnt'high = 18 and byte_cnt'low = 3 severity failure;
        assert byte_cnt'left = 18 and byte_cnt'right = 3 severity failure;
        assert low_part'length = 3 and low_part'high = 2 severity failure;
        cnt <= to_unsigned(16#4_0005#, 19);
        wait for 1 ns;
        assert byte_cnt = to_unsigned(16#8000#, 16) severity failure;
        assert low_part = "101" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
