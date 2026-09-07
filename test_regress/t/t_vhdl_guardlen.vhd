-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A subprogram written against an unconstrained array is compiled once per
-- call site, and inside such a copy the parameter's length is what that call
-- supplied rather than anything the design wrote. So a call the copy's own
-- length rules out may still be written, guarded by a test on that length --
-- and whether the actual matches the formal's constrained subtype is a
-- condition to be met while running (4.2.2.1), not an error in the source.
--
-- VUnit's event package is written exactly this way: "if event'length =
-- event_length then ... end if; return basic_event_name(event);", where
-- basic_event_name takes a five-element subtype.

entity t_vhdl_guardlen is
end entity;

architecture a of t_vhdl_guardlen is

    subtype small_t is bit_vector (0 to 4);

    function narrow (v : small_t) return integer is
    begin
        return 5;
    end function;

    -- One copy of this is made per call site, with v's length fixed
    function pick (v : bit_vector) return integer is
    begin
        if v'length /= 5 then
            return v'length;
        end if;
        return narrow (v);
    end function;

    -- The same shape where the guard is the other way round
    function pick_early (v : bit_vector) return integer is
    begin
        if v'length = 5 then
            return narrow (v);
        end if;
        return 0;
    end function;

begin

    main: process is
        variable big   : bit_vector (0 to 33) := (others => '0');
        variable exact : bit_vector (0 to 4)  := (others => '0');
    begin
        assert pick (big) = 34 report "long, guarded away" severity failure;
        assert pick (exact) = 5 report "exactly five" severity failure;
        assert pick_early (big) = 0 report "long, guarded away early" severity failure;
        assert pick_early (exact) = 5 report "exactly five, early" severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
        wait;
    end process;

end architecture;
