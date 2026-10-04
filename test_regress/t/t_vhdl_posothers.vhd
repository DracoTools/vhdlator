-- DESCRIPTION: Verilator: VHDL positional aggregate with "others", generic length
--
-- "('1', others => '0')" for an object whose length names a generic of the
-- top entity: the positional elements take the places from the left, and
-- "others" the rest, however many the run makes it (9.3.3.2). GHDL gna
-- issue44 writes it for the most negative value of a fixed-point word; in
-- VHDL-2008 a positional element may also be a slice, which takes as many
-- places as it is long (9.3.3.3), as gna issue1704 writes it.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_posothers is
    generic (w : integer := 4);
    port (x : in signed(w - 1 downto 0) := (others => '0'));
end entity;

architecture test of t_vhdl_posothers is
    constant c1 : signed(x'range) := ('1', others => '0');
    constant c2 : std_logic_vector(w - 1 downto 0) := ('1', '0', '1', others => '0');
    constant c3 : bit_vector(0 to w - 1) := ('0', '1', others => '1');
    signal s : std_logic_vector(w downto 1) := ('1', others => '0');

    subtype word is bit_vector(w - 1 downto 0);
    function f return word is
    begin
        return ("10", '0', others => '1');
    end function;
begin
    process is
        variable v : std_logic_vector(w - 1 downto 0);
    begin
        assert c1 = "100000" severity failure;
        assert c1 = to_signed(-2 ** (w - 1), w) severity failure;
        assert c2 = "101000" severity failure;
        assert c3 = "011111" severity failure;
        assert c3(0) = '0' and c3(1) = '1' severity failure;
        assert s = "100000" severity failure;
        assert s(w) = '1' and s(1) = '0' severity failure;
        v := ('0', others => '1');
        assert v = "011111" severity failure;
        v := (c2(w - 1 downto w - 3), "1", others => '0');
        assert v = "101100" severity failure;
        assert f = "100111" severity failure;
        wait for 1 ns;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
