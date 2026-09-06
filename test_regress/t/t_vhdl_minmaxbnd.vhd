-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- MINIMUM and MAXIMUM are declared for every scalar type by the language
-- itself (5.2.6), so a call to one whose arguments are static is static in
-- its turn (9.4.2 e), and may be written where a bound has to be known.
-- That is how ieee.float_generic_pkg and ieee.fixed_generic_pkg size the
-- working values of every arithmetic operation they define.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_minmaxbnd is
end entity;

architecture a of t_vhdl_minmaxbnd is

    type light is (red, amber, green);

    -- The shape ieee.float_generic_pkg "add" is written in: the length of
    -- every working value comes from MAXIMUM over the two operands' bounds
    function widen (l, r : signed) return signed is
        constant w   : natural := maximum(l'high, r'high);
        variable acc : signed (w downto 0);
        variable tmp : signed (w-1 downto 0);
    begin
        tmp := (others => '1');
        acc := signed(tmp(w-1) & tmp);
        return acc;
    end function;

    -- and reached through a wrapper that only passes the call along, which
    -- is all ieee.float_generic_pkg's "+" does
    function widen_via (l, r : signed) return signed is
    begin
        return widen(l, r);
    end function;

    -- MINIMUM decides a bound just as MAXIMUM does
    function narrow (l, r : unsigned) return unsigned is
        constant w : natural := minimum(l'length, r'length);
        variable v : unsigned (w-1 downto 0);
    begin
        v := (others => '1');
        return v;
    end function;

    -- Named association reaches the same operation, and has to decide a
    -- bound for the fold to be what makes this compile
    function named_max (l, r : signed) return signed is
        constant w   : natural := maximum(r => r'high, l => l'high);
        variable acc : signed (w downto 0);
        variable tmp : signed (w-1 downto 0);
    begin
        tmp := (others => '0');
        acc := signed(tmp(w-1) & tmp);
        return acc;
    end function;

    -- What already worked, so a fix cannot be a coincidence
    function via_operators (l, r : signed) return integer is
        constant w : natural := l'high + r'high;
    begin
        return w;
    end function;

begin

    p: process is
        variable x, y : signed(7 downto 0)   := "00000011";
        variable s    : signed(3 downto 0)   := "0001";
        variable u    : unsigned(5 downto 0) := "000011";
        variable v    : unsigned(9 downto 0) := (others => '0');
        variable z    : signed(7 downto 0);
    begin
        -- A bound written with MAXIMUM over two operands
        z := widen(x, y);
        assert z'length = 8 report "widen length" severity failure;
        assert to_integer(z) = -1 report "widen value" severity failure;

        -- and the same reached through a wrapper
        z := widen_via(x, y);
        assert to_integer(z) = -1 report "widen_via value" severity failure;

        -- Operands of unequal length: MAXIMUM picks the longer bound
        z := widen(x, s);
        assert z'length = 8 report "widen mixed length" severity failure;

        -- MINIMUM picks the shorter
        assert narrow(u, v)'length = 6 report "narrow length" severity failure;

        -- Named association
        assert named_max(x, s)'length = 8 report "named max length" severity failure;
        assert to_integer(named_max(x, s)) = 0 report "named max value" severity failure;

        -- Over an enumeration the comparison is by position (5.2.6)
        assert maximum(red, green) = green report "enum maximum" severity failure;
        assert minimum(amber, red) = red report "enum minimum" severity failure;

        -- Over a real, and over time, which is ordered by the value scaled
        -- to its primary unit
        assert maximum(1.5, 2.25) = 2.25 report "real maximum" severity failure;
        assert minimum(10 ns, 3 ns) = 3 ns report "time minimum" severity failure;

        -- What already worked
        assert via_operators(x, s) = 10 report "via operators" severity failure;

        report "*-* All Finished *-*";
        wait;
    end process;

end architecture;
