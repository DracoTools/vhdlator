-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package ccpk is
    type ctr_t is protected
        procedure bump;
        impure function count return integer;
    end protected;

    -- Large enough that it is not folded into its caller, and reading state
    -- of its own, so a process made for it would run again on every change
    -- were it sensitive to what it reads rather than to its parameters
    procedure construct (variable c : inout ctr_t; constant tag : string);
    procedure watch (signal s : in bit; variable c : inout ctr_t);
end package;

package body ccpk is
    type ctr_t is protected body
        variable n : integer := 0;
        procedure bump is begin n := n + 1; end;
        impure function count return integer is begin return n; end;
    end protected body;

    procedure construct (variable c : inout ctr_t; constant tag : string) is
        variable txt : string(1 to tag'length) := tag;
    begin
        c.bump;
        report "construct " & txt & " run " & integer'image(c.count);
        -- A procedure may wait, and then the process is inside it, never to
        -- come back
        wait;
    end procedure;

    procedure watch (signal s : in bit; variable c : inout ctr_t) is
    begin
        c.bump;
    end procedure;
end package body;

use work.ccpk.all;

entity t_vhdl_conccall is
end entity;

architecture tb of t_vhdl_conccall is
    shared variable one : ctr_t;
    shared variable two : ctr_t;
    signal tick : bit := '0';
begin
    -- No signal parameter, so the equivalent process ends with a wait that
    -- has no sensitivity clause: it runs once and never again (11.4)
    construct(one, "one");
    construct(two, "two");

    -- A signal parameter, so this one runs again whenever the signal changes
    watch(tick, one);

    p_clk : process is
    begin
        for i in 1 to 3 loop
            wait for 10 ns;
            tick <= not tick;
        end loop;
        wait for 10 ns;

        assert one.count = 5
            report "one.count = " & integer'image(one.count) severity failure;
        assert two.count = 1
            report "two.count = " & integer'image(two.count) severity failure;
        report "*-* All Finished *-*";
        std.env.stop;
    end process;
end architecture;
