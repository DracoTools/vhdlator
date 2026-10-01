-- DESCRIPTION: Verilator: VHDL logical operators on arrays as long as running decides
--
-- "and", "or", "xor" and their negations between two arrays whose length
-- only running decides are taken element by element (9.2.2).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_bit.all;

entity t_vhdl_dynlogic is
end entity;

architecture test of t_vhdl_dynlogic is

    -- The low byte, as VUnit's codec takes it
    function low_byte (value : bit_vector) return integer is
        variable v : unsigned(value'length - 1 downto 0) := unsigned(value);
    begin
        return to_integer(v and to_unsigned(255, v'length));
    end function;

    function mix (a, b : bit_vector; sel : natural) return bit_vector is
        variable x : bit_vector(a'length - 1 downto 0) := a;
        variable y : bit_vector(b'length - 1 downto 0) := b;
    begin
        case sel is
            when 0 => return x and y;
            when 1 => return x or y;
            when 2 => return x xor y;
            when 3 => return x nand y;
            when 4 => return x nor y;
            when others => return x xnor y;
        end case;
    end function;

    function chain (a : bit_vector) return bit_vector is
        variable x : bit_vector(a'length - 1 downto 0) := a;
    begin
        return (x or not a) xor a;
    end function;

    function slv_and (a, b : std_logic_vector) return std_logic_vector is
        variable x : std_logic_vector(a'length - 1 downto 0) := a;
    begin
        return x and b;
    end function;

begin

    process is
        variable bv : bit_vector(0 to 11) := x"A5C";
        variable n : natural := 12;
        variable r : bit_vector(0 to 11);
        variable s : std_logic_vector(7 downto 0);
        variable m : natural := 8;
        variable k : std_logic_vector(1 to 8) := "10100101";
    begin
        assert low_byte(bv(0 to n - 1)) = 16#5C# severity failure;
        r := mix(bv(0 to n - 1), x"0F3", 0);
        assert r = x"050" severity failure;
        r := mix(bv(0 to n - 1), x"0F3", 1);
        assert r = x"AFF" severity failure;
        r := mix(bv(0 to n - 1), x"0F3", 2);
        assert r = x"AAF" severity failure;
        r := mix(bv(0 to n - 1), x"0F3", 3);
        assert r = x"FAF" severity failure;
        r := mix(bv(0 to n - 1), x"0F3", 4);
        assert r = x"500" severity failure;
        r := mix(bv(0 to n - 1), x"0F3", 5);
        assert r = x"550" severity failure;
        r := chain(bv(0 to n - 1));
        assert r = not bv severity failure;
        s := slv_and(x"C3", k(1 to m));
        assert s = x"81" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
