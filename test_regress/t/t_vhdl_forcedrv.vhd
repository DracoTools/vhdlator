-- DESCRIPTION: Verilator: VHDL force of a signal with drivers of its own
--
-- A force overrides what a signal's drivers add up to, and a release hands
-- it back to them (10.5.2.1): for a signal several processes drive, and for
-- one a procedure forces through a signal parameter and nothing forces by
-- name.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_forcedrv is
end entity;

architecture test of t_vhdl_forcedrv is
    signal a : std_logic := '0';                       -- forced in a procedure only
    signal b : std_logic := '0';                       -- driven here and by the procedure
    signal c : std_logic := '0';                       -- two drivers, forced by name
    signal v : std_logic_vector(3 downto 0) := "0101";

    procedure pulse (signal target : out std_logic) is
    begin
        target <= force '1';
        wait for 1 ps;
        target <= release;
        wait for 1 ps;
    end procedure;

    procedure hold (signal t : out std_logic_vector; constant on_not_off : boolean) is
    begin
        if on_not_off then
            t <= force "1111";
        else
            t <= release;
        end if;
        wait for 1 ps;
    end procedure;
begin
    b <= '0';
    c <= '0';

    main : process is
    begin
        pulse(a);
        assert a = '0' report "a released" severity failure;
        pulse(target => b);
        assert b = '0' report "b released" severity failure;
        hold(v, true);
        assert v = "1111" report "v forced" severity failure;
        hold(v, false);
        assert v = "0101" report "v released" severity failure;

        c <= 'Z';
        wait for 1 ps;
        assert c = '0' report "c resolved" severity failure;
        c <= force '1';
        wait for 1 ps;
        assert c = '1' report "c forced" severity failure;
        c <= release;
        wait for 1 ps;
        assert c = '0' report "c released" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

    watch : process is
    begin
        wait for 500 fs;
        assert a = '1' report "a during the pulse" severity failure;
        wait for 1 ps;
        assert a = '0' report "a after the pulse" severity failure;
        wait;
    end process;

end architecture;
