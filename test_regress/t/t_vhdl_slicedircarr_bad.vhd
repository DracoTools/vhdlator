-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A slice must run the way its prefix does (8.5). A parameter handed a
-- value whose length only running decides carries its bounds with it, so
-- which way it runs is known only while running, and so is the error. A
-- null slice is not one.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_slicedircarr_bad is
end entity;

architecture test of t_vhdl_slicedircarr_bad is
    function lo4(src : std_logic_vector) return std_logic_vector is
    begin
        return src(3 downto 0);
    end function;
    function none(src : std_logic_vector) return natural is
    begin
        return src(0 downto 1)'length;
    end function;
begin
    process
        variable up : std_logic_vector(0 to 15) := x"1234";
        variable dn : std_logic_vector(15 downto 0) := x"1234";
        variable n : natural := 11;
    begin
        assert lo4(dn(n downto 0)) = x"4" report "lo4 gave " & to_hstring(lo4(dn(n downto 0)))
            severity failure;
        assert none(up(0 to n)) = 0 severity failure;
        report "slices that match passed";
        -- up(0 to n) runs 'to'
        report "reached " & to_hstring(lo4(up(0 to n)));
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
