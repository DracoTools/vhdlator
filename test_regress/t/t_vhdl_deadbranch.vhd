-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

package dbpk is
    -- The lengths of the two are known only at the call, and the branch that
    -- copies one straight into the other is reached only when they agree.
    -- Where they do not, that branch is never run, and what it assigns is
    -- checked for length when it runs and not before (10.5.2.1)
    function normalize (constant v : std_logic_vector;
                        constant t : std_logic_vector) return std_logic_vector;
end package;

package body dbpk is
    function normalize (constant v : std_logic_vector;
                        constant t : std_logic_vector) return std_logic_vector is
        alias av : std_logic_vector(v'length - 1 downto 0) is v;
        alias at : std_logic_vector(t'length - 1 downto 0) is t;
        variable r : std_logic_vector(t'length - 1 downto 0);
    begin
        if av'length > at'length then
            r := av(at'length - 1 downto 0);
        elsif av'length = at'length then
            r := av;
        else
            r                          := (others => '0');
            r(av'length - 1 downto 0)  := av;
        end if;
        return r;
    end function;
end package body;

use work.dbpk.all;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_deadbranch is
end entity;

architecture tb of t_vhdl_deadbranch is
    signal wide   : std_logic_vector(31 downto 0) := (others => '0');
    signal narrow : std_logic_vector(2 downto 0)  := "101";
    signal byte   : std_logic_vector(7 downto 0)  := "00001111";
begin
    process is
        variable r : std_logic_vector(31 downto 0);
    begin
        r := normalize(narrow, wide);
        assert r = x"00000005" report "narrow -> wide wrong" severity failure;

        r := normalize(byte, wide);
        assert r = x"0000000f" report "byte -> wide wrong" severity failure;

        r := normalize(wide, wide);
        assert r = x"00000000" report "wide -> wide wrong" severity failure;

        assert normalize(wide, byte) = x"00" report "wide -> byte wrong" severity failure;

        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
