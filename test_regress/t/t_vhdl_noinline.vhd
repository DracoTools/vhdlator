-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package pk is
    -- A shared variable is of a protected type (6.4.2.4)
    type counter_t is protected
        procedure add (constant n : natural);
        impure function get return natural;
    end protected;
    shared variable calls : counter_t;
    procedure note (constant n : natural);
    function twice (constant n : natural) return natural;
end package;

package body pk is
    type counter_t is protected body
        variable total : natural := 0;
        procedure add (constant n : natural) is
        begin
            total := total + n;
        end procedure;
        impure function get return natural is
        begin
            return total;
        end function;
    end protected body;

    -- Reaches an object the package declares, which is one object for the
    -- whole design: a subprogram emitted as a function of its own still
    -- names it
    procedure note (constant n : natural) is
    begin
        calls.add(n);
    end procedure;

    function twice (constant n : natural) return natural is
    begin
        return n + n;
    end function;
end package body;

use work.pk.all;

entity t_vhdl_noinline is
end entity;

architecture test of t_vhdl_noinline is
    signal q : natural := 0;

    -- A subprogram calling a subprogram calling a subprogram, which is the
    -- shape that makes writing every body out at every call site multiply
    function inner (constant n : natural) return natural is
    begin
        return twice(n) + 1;
    end function;

    function middle (constant n : natural) return natural is
    begin
        return inner(n) + inner(n + 1) + inner(n + 2);
    end function;

    function outer (constant n : natural) return natural is
    begin
        return middle(n) + middle(n + 1);
    end function;

    -- One that writes a signal, so it is a procedure with an effect
    procedure bump (signal s : inout natural; constant n : natural) is
    begin
        s <= s + n;
        note(n);
    end procedure;
begin
    main : process is
        variable v : natural;
    begin
        v := outer(1);
        -- middle(1) = inner(1)+inner(2)+inner(3) = 3+5+7 = 15
        -- middle(2) = inner(2)+inner(3)+inner(4) = 5+7+9 = 21
        assert v = 36 report "outer(1)=" & natural'image(v) severity failure;

        bump(q, 5);
        wait for 1 ns;
        assert q = 5 report "q=" & natural'image(q) severity failure;
        bump(q, 7);
        wait for 1 ns;
        assert q = 12 report "q=" & natural'image(q) severity failure;

        assert calls.get = 12 report "calls=" & natural'image(calls.get) severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
