-- DESCRIPTION: Verilator: VHDL expanded name of an overloaded enumeration
-- literal as an actual
--
-- Two enumeration types of one package both declare START and IDLE, and a
-- function is overloaded on each. "mux(v, work.p.start, work.p.idle)"
-- assigned to a signal of one of the types picks that overload, and with it
-- which START each expanded name denotes, as the simple names do (12.5; nvc
-- sem murax).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_selenumovl_pkg is
    type tx_t is (IDLE, START, STOP);
    type rx_t is (IDLE, START, STOP);
    function mux (sel : bit; one : tx_t; zero : tx_t) return tx_t;
    function mux (sel : bit; one : rx_t; zero : rx_t) return rx_t;
end package;

package body t_vhdl_selenumovl_pkg is
    function mux (sel : bit; one : tx_t; zero : tx_t) return tx_t is
    begin
        if sel = '1' then
            return one;
        else
            return zero;
        end if;
    end function;

    function mux (sel : bit; one : rx_t; zero : rx_t) return rx_t is
    begin
        if sel = '1' then
            return one;
        else
            return zero;
        end if;
    end function;
end package body;

use work.t_vhdl_selenumovl_pkg.all;

entity t_vhdl_selenumovl is
end entity;

architecture test of t_vhdl_selenumovl is
    signal s : tx_t := STOP;
    signal r : rx_t := STOP;
    signal v : bit := '1';
begin
    process is
    begin
        s <= mux(v, work.t_vhdl_selenumovl_pkg.START, work.t_vhdl_selenumovl_pkg.IDLE);
        r <= mux(v, work.t_vhdl_selenumovl_pkg.IDLE, work.t_vhdl_selenumovl_pkg.START);
        wait for 1 ns;
        assert s = START report "s" severity failure;
        assert r = IDLE report "r" severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
