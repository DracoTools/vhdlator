-- DESCRIPTION: Verilator: VHDL generic subprogram given a generic value only a call works out
--
-- "init => to_unsigned(0, 4)" is a value of the generic type, computed by
-- a function, and the instantiated subprogram starts from it (6.5.5, 4.4).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.numeric_std.all;

entity t_vhdl_gensubinit is
end entity;

architecture test of t_vhdl_gensubinit is

    function generic_sum
        generic (
            type t;
            init : t;
            function "+" (l : t; r : integer) return t is <>)
        return t
    is
        variable rv : init'subtype := init;
    begin
        for i in 1 to 3 loop
            rv := rv + 1;
        end loop;
        return rv;
    end function;

    function "+" (l : real; r : integer) return real is
    begin
        return l + real(r);
    end function;

    function sum is new generic_sum generic map (
        t => unsigned,
        init => to_unsigned(5, 4));
    function sum is new generic_sum generic map (
        t => integer,
        init => 10);
    function sum is new generic_sum generic map (
        t => real,
        init => 0.5);

begin

    p1 : process is
        variable us : unsigned(3 downto 0);
        variable iv : integer;
        variable rv : real;
    begin
        us := sum;
        iv := sum;
        rv := sum;
        assert us = to_unsigned(8, 4) severity failure;
        assert iv = 13 severity failure;
        assert rv = 3.5 severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
