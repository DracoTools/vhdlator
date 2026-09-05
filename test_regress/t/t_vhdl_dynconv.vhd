-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_dynconv is
end entity;

architecture a of t_vhdl_dynconv is
    signal howmany : natural := 8;

    -- The parameter says how long it is, so the value carries its own bounds
    -- and nothing it is given to has a width the conversion could take
    function build (size : natural) return unsigned is
        constant half : integer := size / 2;
        variable rv : unsigned(1 to size) := (others => '0');
    begin
        rv(1 to half) := to_unsigned(5, half);
        return rv;
    end function;

    function build_signed (size : natural) return signed is
        variable rv : signed(1 to size) := (others => '0');
    begin
        rv := to_signed(-3, size);
        return rv;
    end function;
begin
    process
        variable v8 : unsigned(1 to 8);
        variable v6 : unsigned(1 to 6);
        variable s8 : signed(1 to 8);
    begin
        v8 := build(8);
        assert v8 = "01010000" report "eight: " & to_string(v8) severity failure;

        v6 := build(6);
        assert v6 = "101000" report "six: " & to_string(v6) severity failure;

        s8 := build_signed(8);
        assert s8 = "11111101" report "signed: " & to_string(s8) severity failure;

        -- And through a length the run settles rather than the text
        v8 := build(howmany);
        assert v8 = "01010000" report "run-time: " & to_string(v8) severity failure;

        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
