-- DESCRIPTION: Verilator: VHDL aggregate whose one choice only running settles
--
-- "(index => '1')" with index a signal: an aggregate of one element
-- association may have a choice that is not static (9.3.3.3), and then runs
-- from that place to it, the way its index subtype runs (GHDL gna
-- issue2581).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_aggonechoice is
end entity;

architecture test of t_vhdl_aggonechoice is
    function get_left (s : std_logic_vector) return natural is
    begin
        return s'left;
    end function;

    function count (s : bit_vector) return natural is
    begin
        return s'length;
    end function;

    signal index, res1, res2 : integer := 0;
begin
    res1 <= get_left((index => '1'));

    rtl : process (index) is
    begin
        res2 <= get_left((index => '1'));
    end process;

    process is
        variable v : natural := 3;
    begin
        for i in 1 to 7 loop
            index <= i;
            wait for 10 ns;
            assert res1 = i severity failure;
            assert res2 = i severity failure;
        end loop;
        assert count((v => '1')) = 1 severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
