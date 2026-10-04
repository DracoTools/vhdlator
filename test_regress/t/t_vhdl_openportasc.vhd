-- DESCRIPTION: Verilator: VHDL open port given an ascending actual, assigned
-- a call's result
--
-- "port (po : out bit_vector)" takes its subtype from its actual (6.5.6.3),
-- which here runs "1 to 4". A function returning an unconstrained
-- BIT_VECTOR is copied into it element by element, and the copy has to run
-- the way the actual does rather than "downto", which the shared copy of
-- the entity assumed.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_openportasc_pkg is
    function bmux (a : bit_vector; b : bit_vector; en : boolean) return bit_vector;
end package;

package body t_vhdl_openportasc_pkg is
    function bmux (a : bit_vector; b : bit_vector; en : boolean) return bit_vector is
    begin
        if en then
            return a;
        else
            return b;
        end if;
    end function;
end package body;

use work.t_vhdl_openportasc_pkg.all;

entity t_vhdl_openportasc_sub is
    port (i1 : in bit_vector; i2 : in bit_vector; en : in boolean; po : out bit_vector);
end entity;

architecture a of t_vhdl_openportasc_sub is
begin
    po <= bmux(i1, i2, en);
end architecture;

entity t_vhdl_openportasc is
end entity;

architecture test of t_vhdl_openportasc is
    signal s4a : bit_vector(1 to 4) := "1100";
    signal s4b : bit_vector(1 to 4) := "0011";
    signal r4 : bit_vector(1 to 4);
    signal s3a : bit_vector(2 downto 0) := "110";
    signal s3b : bit_vector(2 downto 0) := "001";
    signal r3 : bit_vector(2 downto 0);
    signal sel : boolean := true;
begin
    u4 : entity work.t_vhdl_openportasc_sub port map (s4a, s4b, sel, r4);
    u3 : entity work.t_vhdl_openportasc_sub port map (s3a, s3b, sel, r3);

    process is
    begin
        wait for 1 ns;
        assert r4 = "1100" report "r4 " & to_string(r4) severity failure;
        assert r3 = "110" report "r3 " & to_string(r3) severity failure;
        sel <= false;
        wait for 1 ns;
        assert r4 = "0011" report "r4 b " & to_string(r4) severity failure;
        assert r3 = "001" report "r3 b " & to_string(r3) severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
