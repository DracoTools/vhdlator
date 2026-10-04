-- DESCRIPTION: Verilator: VHDL constants of an open subtype given what a call returns
--
-- A constant whose subtype is left open takes the bounds of its value
-- (5.3.2.2): an array of vectors keeps the index range the function's
-- result was declared with, rather than starting at the index subtype's
-- left; and a record whose element is open, given a record a function
-- built with that element fixed, takes the element as an assignment would.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_opencall is
end entity;

architecture test of t_vhdl_opencall is
    type indices_t is array (natural range <>) of std_logic_vector;

    function init (b : indices_t) return indices_t is
        variable result : indices_t(b'range)(b'element'range);
    begin
        result := (others => (others => '0'));
        return result;
    end function;

    function down (n : natural) return indices_t is
        variable result : indices_t(n + 2 downto 3)(0 to 2);
    begin
        result := (others => "101");
        return result;
    end function;

    type rec_t is record
        v : bit_vector;
        k : integer;
    end record;

    function mk (n : natural) return rec_t is
        variable r : rec_t(v(n downto 1));
    begin
        r.v := (others => '1');
        r.k := n;
        return r;
    end function;

    constant a1 : indices_t := (1 to 4 => "01");
    constant a2 : indices_t := init(a1);
    constant a3 : indices_t := down(4);
    constant c : rec_t := mk(3);
begin
    process is
        constant pc : rec_t := mk(2);
    begin
        assert a2'left = 1 and a2'right = 4 and a2'length = 4 severity failure;
        assert a2(3) = "00" and a2(1)'length = 2 severity failure;
        assert a3'left = 6 and a3'right = 3 and not a3'ascending severity failure;
        assert a3(5) = "101" severity failure;
        assert c.v'left = 3 and c.v = "111" and c.k = 3 severity failure;
        assert pc.v = "11" and pc.k = 2 severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
