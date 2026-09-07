-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A call to a pure function whose actuals are static is itself a static
-- expression (9.4.3 i), so it may be written wherever a bound has to be
-- known. IEEE's floating-point package sizes the fraction of every one of
-- its operations that way -- "constant fraction_width : NATURAL :=
-- -mine(l'low, r'low)", where "mine" is an ordinary function declared beside
-- it -- and until such a call folds, none of those bounds is a number.
--
-- A bound that does not fold is not always fatal, since a value can be
-- carried with its bounds instead; what it costs is every operation that
-- needs a width written down. The concatenation below is one of those, and
-- is the shape that stops ieee.float_generic_pkg at "or (remainder & sticky)".

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package t_vhdl_funcfold_pkg is
    function mine (l, r : integer) return integer;
    function bits_for (n : natural) return natural;
    function weigh (kind : character; n : natural) return natural;
    function scaled (n : natural; by : natural := 3) return natural;
    function fact (n : natural) return natural;
    function two_step (n : natural) return natural;
    function widen (arg : signed) return std_ulogic;
end package;

package body t_vhdl_funcfold_pkg is

    -- The shape ieee.float_generic_pkg declares "mine" in: a guard, a report
    -- that is not reached, and a call to a predefined operation
    function mine (l, r : integer) return integer is
    begin
        if l = integer'low or r = integer'low then
            report "unbounded number passed" severity error;
            return 0;
        end if;
        return minimum (l, r);
    end function;

    -- A while loop, an exit, and a local variable read after it is written
    function bits_for (n : natural) return natural is
        variable acc : natural;
        variable i   : natural;
    begin
        acc := 0;
        i   := n;
        while i > 0 loop
            acc := acc + 1;
            i   := i / 2;
            if acc > 30 then
                exit;
            end if;
        end loop;
        return acc;
    end function;

    -- A for loop over a range the call decides, and a case statement
    function weigh (kind : character; n : natural) return natural is
        variable acc : natural := 0;
    begin
        for i in 1 to n loop
            case kind is
                when 'a'    => acc := acc + i;
                when 'b'    => acc := acc + 2;
                when others => acc := acc + 0;
            end case;
        end loop;
        return acc;
    end function;

    -- A formal the call leaves out takes its default, and one function calls
    -- another
    function scaled (n : natural; by : natural := 3) return natural is
    begin
        return mine (n * by, 64);
    end function;

    -- A function that calls itself
    function fact (n : natural) return natural is
    begin
        if n <= 1 then
            return 1;
        end if;
        return n * fact (n - 1);
    end function;

    -- A local constant whose value is itself a call, so one activation is
    -- started while the one that names it is still being filled in
    function two_step (n : natural) return natural is
        constant half : natural := bits_for (n);
        variable acc  : natural;
    begin
        acc := half * 2;
        return acc;
    end function;

    -- The shape that needs the fold to compile at all: a working value whose
    -- length comes from a call to a helper, then written into a
    -- concatenation, which has to know how many elements it is joining (9.2.5)
    function widen (arg : signed) return std_ulogic is
        constant w    : natural := mine (arg'length, two_step (2));
        constant base : signed (w-1 downto 0) := to_signed (3, w);
    begin
        return or (std_ulogic_vector (base) & '0');
    end function;

end package body;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.t_vhdl_funcfold_pkg.all;

entity t_vhdl_funcfold is
end entity;

architecture a of t_vhdl_funcfold is

    -- Every one of these decides a bound
    constant W1 : natural := -mine (-8, -3);          -- 8
    constant W2 : natural := bits_for (100);          -- 7
    constant W3 : natural := weigh ('a', 4);          -- 10
    constant W4 : natural := scaled (5);              -- 15
    constant W5 : natural := scaled (5, by => 20);    -- 64
    constant W6 : natural := fact (5);                -- 120
    constant W7 : natural := two_step (100);          -- 14

    signal s1 : std_logic_vector (W1 - 1 downto 0) := (others => '0');
    signal s2 : std_logic_vector (W2 - 1 downto 0) := (others => '0');
    signal s3 : unsigned (W3 - 1 downto 0)         := (others => '0');
    signal s4 : signed (W4 - 1 downto 0)           := (others => '0');
    signal s5 : std_logic_vector (W5 - 1 downto 0) := (others => '0');
    signal s6 : std_logic_vector (W6 - 1 downto 0) := (others => '0');
    signal s7 : std_logic_vector (W7 - 1 downto 0) := (others => '0');

begin

    main: process is
        variable v : signed (7 downto 0) := (others => '0');
    begin
        assert s1'length = 8 report "bad W1" severity failure;
        assert s2'length = 7 report "bad W2" severity failure;
        assert s3'length = 10 report "bad W3" severity failure;
        assert s4'length = 15 report "bad W4" severity failure;
        assert s5'length = 64 report "bad W5" severity failure;
        assert s6'length = 120 report "bad W6" severity failure;
        assert s7'length = 14 report "bad W7" severity failure;

        -- The same functions still run, and give the same answers
        assert mine (-8, -3) = -8 report "bad mine" severity failure;
        assert bits_for (100) = 7 report "bad bits_for" severity failure;
        assert weigh ('a', 4) = 10 report "bad weigh" severity failure;
        assert weigh ('b', 4) = 8 report "bad weigh b" severity failure;
        assert scaled (5) = 15 report "bad scaled" severity failure;
        assert fact (5) = 120 report "bad fact" severity failure;
        assert two_step (100) = 14 report "bad two_step" severity failure;

        assert widen (v) = '1' report "bad widen" severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
        wait;
    end process;

end architecture;
