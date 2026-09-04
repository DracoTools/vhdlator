-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

package genpk is
    -- Unconstrained, so a call that cannot say how long its argument is
    -- carries the bounds with the value
    function ones (constant v : std_logic_vector) return integer;
    function widen (constant v : std_logic_vector) return std_logic_vector;
    procedure snapshot (signal s : in std_logic_vector; variable n : out integer;
                        variable txt : out string);
end package;

package body genpk is
    function ones (constant v : std_logic_vector) return integer is
        variable c : integer := 0;
    begin
        for i in v'range loop
            if v(i) = '1' then
                c := c + 1;
            end if;
        end loop;
        return c;
    end function;

    function widen (constant v : std_logic_vector) return std_logic_vector is
    begin
        return v & v;
    end function;

    -- A signal-class parameter: how long it is is the caller's signal's
    -- business, and 'LAST_VALUE of it is that long too
    procedure snapshot (signal s : in std_logic_vector; variable n : out integer;
                        variable txt : out string) is
        variable was : std_logic_vector(s'range) := s'last_value;
    begin
        n := ones(was);
        txt(1 to s'length) := to_string(was);
    end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.genpk.all;

entity t_vhdl_genlen is
    generic (GC_WIDTH : integer := 6);
end entity;

architecture test of t_vhdl_genlen is
    -- A length settled by elaboration but written against a generic, which
    -- is the ordinary way RTL is written
    signal d : std_logic_vector(GC_WIDTH - 1 downto 0) := (others => '0');
    -- and a variable of that length given a value that carries its own
    signal ready : boolean := false;
begin
    d <= "101101" after 1 ns;

    process
        variable seen : integer;
        variable text : string(1 to GC_WIDTH);
        variable wide : std_logic_vector(2 * GC_WIDTH - 1 downto 0);
    begin
        wait for 2 ns;
        -- A signal of a generic length handed to an unconstrained parameter
        if ones(d) /= 4 then
            report "%Error: ones(d)" severity failure;
        end if;
        -- and a value that carries its own bounds put into one that does not
        wide := widen(d);
        if wide /= "101101101101" then
            report "%Error: widen" severity failure;
        end if;
        -- 'LAST_VALUE of a signal-class parameter is as long as the signal
        snapshot(d, seen, text);
        if seen /= 0 then
            report "%Error: last_value had " & integer'image(seen) & " ones" severity failure;
        end if;
        if text /= "000000" then
            report "%Error: last_value text [" & text & "]" severity failure;
        end if;

        report "*-* All Finished *-*";
        std.env.stop;
    end process;
end architecture;
