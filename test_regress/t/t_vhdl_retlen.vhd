-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_retlen is
end entity;

architecture test of t_vhdl_retlen is

    -- A guard returning a null value says nothing about how long the result
    -- is on the paths that have one (4.2.1)
    function bumped (v : std_logic_vector) return std_logic_vector is
        constant nul : std_logic_vector(0 downto 1) := (others => '0');
    begin
        if v'length < 1 then
            return nul;
        end if;
        return std_logic_vector(unsigned(v) + 1);
    end function;

    -- Two returns of different lengths: the result is as long as whichever
    -- path ran, which only running decides
    function chosen (n : natural) return std_logic_vector is
    begin
        case n is
            when 0 => return "00";
            when 1 => return "111";
            when others => return "10101";
        end case;
    end function;

    signal d : natural := 1;

begin

    main : process is
        variable v : std_logic_vector(7 downto 0) := x"41";
    begin
        assert bumped(v) = x"42" report "bumped" severity failure;
        assert bumped(v)'length = 8 report "bumped length" severity failure;

        assert chosen(0)'length = 2 report "chosen 0" severity failure;
        assert chosen(d) = "111" report "chosen d" severity failure;
        assert chosen(2)'length = 5 report "chosen 2" severity failure;

        -- numeric_std compares a vector with an integer by value
        assert bumped(v) = x"42" report "value compare" severity failure;
        -- an integer wider than the vector is not equal to any of its values
        assert unsigned(v) /= 321 report "wide compare" severity failure;
        assert unsigned(chosen(d)) = 7 report "carried compare" severity failure;
        assert unsigned(chosen(d)) /= 6 report "carried compare ne" severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
