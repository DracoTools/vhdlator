-- DESCRIPTION: Verilator: VHDL generic type given a subtype attribute
--
-- "T => foo'subtype" gives a generic type the subtype of an object,
-- constraint and all, as an attribute may stand wherever a type mark does
-- (16.2.5); 'ELEMENT gives an array's element subtype (GHDL gna issue3098).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_gentypeattr_sub is
    generic (type t);
    port (port_in : in t; port_out : out t);
end entity;

architecture arch of t_vhdl_gentypeattr_sub is
begin
    port_out <= port_in;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_gentypeattr is
end entity;

architecture test of t_vhdl_gentypeattr is
    type word_arr is array (0 to 1) of std_logic_vector(5 downto 0);
    signal a, b : std_logic;
    signal v, w : std_logic_vector(11 downto 4);
    signal arr : word_arr;
    signal e, f : std_logic_vector(5 downto 0);
begin
    u1 : entity work.t_vhdl_gentypeattr_sub generic map (t => a'subtype) port map (a, b);
    u2 : entity work.t_vhdl_gentypeattr_sub generic map (t => v'subtype) port map (v, w);
    u3 : entity work.t_vhdl_gentypeattr_sub generic map (t => arr'element) port map (e, f);

    process is
    begin
        a <= '1';
        v <= x"A5";
        e <= "101101";
        wait for 1 ns;
        assert b = '1' severity failure;
        assert w = x"A5" and w'left = 11 and w'length = 8 severity failure;
        assert f = "101101" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
