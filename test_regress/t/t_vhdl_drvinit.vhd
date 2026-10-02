-- DESCRIPTION: Verilator: VHDL initial value of a signal with several drivers
--
-- Each driver of a signal holds the signal's initial value to begin with
-- (14.7.3.2), so the elements no driver ever assigns keep what the
-- declaration gave them -- whether that is a literal or a function's result.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_drvinit is
end entity;

architecture test of t_vhdl_drvinit is

    subtype ev_t is std_logic_vector(0 to 4);

    -- Two elements the drivers share, and an identifier nothing assigns
    impure function new_ev (id : natural) return ev_t is
        variable v : ev_t;
    begin
        v(0 to 1) := "ZZ";
        v(2 to 4) := std_logic_vector(to_unsigned(id, 3));
        return v;
    end function;

    function id_of (signal e : ev_t) return natural is
    begin
        return to_integer(unsigned(e(2 to 4)));
    end function;

    procedure activate (signal e : inout std_logic_vector) is
    begin
        e(e'left) <= '1';
    end procedure;

    procedure let_go (signal e : inout std_logic_vector) is
    begin
        e(e'left) <= 'Z';
    end procedure;

    signal e1 : ev_t := new_ev(4);     -- two drivers, a function's result
    signal e2 : ev_t := "ZZ101";       -- two drivers, a literal
    signal e3 : ev_t := new_ev(3);     -- one driver
    signal one : std_logic := '1';     -- two drivers of one bit

begin

    main : process is
    begin
        wait for 0 ns;
        assert id_of(e1) = 4 report "e1 to begin with" severity failure;
        assert id_of(e2) = 5 report "e2 to begin with" severity failure;
        assert id_of(e3) = 3 report "e3 to begin with" severity failure;
        assert one = '1' report "one to begin with" severity failure;
        activate(e1);
        activate(e3);
        wait for 1 ns;
        assert e1(0) = '1' and id_of(e1) = 4 report "e1 driven" severity failure;
        assert e3(0) = '1' and id_of(e3) = 3 report "e3 driven" severity failure;
        let_go(e1);
        wait for 10 ns;
        assert e1(0) = '1' and id_of(e1) = 4 report "e1 held by the other" severity failure;
        assert e2(0) = '1' and id_of(e2) = 5 report "e2 driven by the other" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

    other : process is
    begin
        wait for 5 ns;
        activate(e1);
        activate(e2);
        one <= '1';
        wait;
    end process;

    -- A second driver of each, which does nothing until then
    third : process is
    begin
        wait for 20 ns;
        let_go(e2);
        one <= '1';
        wait;
    end process;

end architecture;
