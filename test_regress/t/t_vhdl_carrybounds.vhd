-- DESCRIPTION: Verilator: VHDL values of one length and different bounds
--
-- Two values as long as each other but numbered differently, each handed to
-- a function beside a value whose bounds only running decides, keep their
-- own bounds. ieee.fixed_pkg reads the bounds as where the binary point is.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.fixed_pkg.all;

entity t_vhdl_carrybounds is
end entity;

architecture test of t_vhdl_carrybounds is

    -- One bit set at the top of a range only running decides
    function mk (l, r : integer) return ufixed is
        variable v : ufixed(l downto r) := (others => '0');
    begin
        v(l) := '1';
        return v;
    end function;

    function same (a, b : bit_vector) return boolean is
    begin
        return a'left = b'left and a'right = b'right and a = b;
    end function;

    function pass (l, r : integer) return bit_vector is
        variable v : bit_vector(l downto r) := (others => '1');
    begin
        return v;
    end function;

begin

    process is
        variable l, r : integer;
        constant c1 : ufixed(3 downto 1) := "100";    -- 8.0
        constant c2 : ufixed(-2 downto -4) := "100";  -- 0.25
        constant b1 : bit_vector(7 downto 5) := "111";
        constant b2 : bit_vector(2 downto 0) := "111";
    begin
        l := 3;
        r := 1;
        assert mk(l, r) = c1 report "8.0" severity failure;
        assert mk(l, r) /= c2 report "8.0 is not 0.25" severity failure;
        assert same(pass(l + 4, r + 4), b1) report "7 downto 5" severity failure;
        l := -2;
        r := -4;
        assert mk(l, r) = c2 report "0.25" severity failure;
        assert mk(l, r) /= c1 report "0.25 is not 8.0" severity failure;
        assert same(pass(l + 4, r + 4), b2) report "2 downto 0" severity failure;
        assert not same(pass(l + 4, r + 4), b1) report "not 7 downto 5" severity failure;
        -- A package's own TO_STRING, which writes the binary point
        assert to_string(c2) = "0.0100" severity failure;
        assert to_string(mk(l, r)) = "0.0100" severity failure;
        l := 3;
        r := -3;
        assert to_string(mk(l, r)) = "1000.000" severity failure;
        report "to_string: " & to_string(mk(l, r));
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
