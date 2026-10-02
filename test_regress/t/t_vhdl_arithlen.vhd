-- DESCRIPTION: Verilator: VHDL length of a package operator's result
--
-- "+" and "-" of numeric_std give a result as long as the longer operand
-- (16.5), whichever side it is on and however the operand's length is known.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_arithlen is
end entity;

architecture test of t_vhdl_arithlen is
    signal five : std_logic_vector(4 downto 0) := "11111";
    signal r1, r2, r3, r4 : std_logic_vector(5 downto 0);

    function pass (x : unsigned) return unsigned is
    begin
        return x;
    end function;
begin
    -- The longer operand on the right, as a function's result
    r1 <= std_logic_vector(unsigned'(unsigned'("00000") - pass(unsigned(five(4) & five))));
    -- On the right, as a concatenation
    r2 <= std_logic_vector(unsigned'("00001") + unsigned(five(4) & five));
    -- On the left
    r3 <= std_logic_vector(unsigned(five(4) & five) + unsigned'("00001"));
    -- And qualified once more round the whole
    r4 <= std_logic_vector'(std_logic_vector(unsigned'("00010") + pass(unsigned('0' & five))));

    process is
    begin
        wait for 1 ns;
        assert r1 = "000001" severity failure;
        assert r2 = "000000" severity failure;
        assert r3 = "000000" severity failure;
        assert r4 = "100001" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
