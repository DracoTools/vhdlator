-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A slice must run the way its prefix does (8.5). Where the prefix is a
-- parameter, which way it runs is what the call gave it, so the slice is an
-- error only in a call that reaches it with the other direction: a design
-- that never makes that call is legal, and one that does fails when it runs.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_slicedirrun_bad is
end entity;

architecture test of t_vhdl_slicedirrun_bad is
    function lo8(src : std_logic_vector) return std_logic_vector is
    begin
        return src(7 downto 0);
    end function;
begin
    process
        variable d : std_logic_vector(23 downto 0) := x"123456";
    begin
        assert lo8(d) = x"56" report "lo8 gave " & to_hstring(lo8(d)) severity failure;
        -- Never reached: d(0) is '0'
        if d(0) = '1' then
            report "unreached " & to_hstring(lo8(x"abcdef"));
        end if;
        report "slices that match passed";
        -- x"123456" is indexed 0 to 23
        report "reached " & to_hstring(lo8(x"123456"));
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
