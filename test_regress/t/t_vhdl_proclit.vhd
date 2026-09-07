-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A procedure call statement names a procedure and nothing else (4.3, 10.7),
-- so nothing else competes with one -- not a function of the same name, and
-- not an enumeration literal of it either. VUnit names the phases of a test
-- run with an enumeration and then declares a procedure per phase, so
-- "test_case_setup" is a literal where a phase is compared and a call where
-- the phase is entered, ten lines apart in the same process.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_proclit is
end entity;

architecture a of t_vhdl_proclit is

    type phase_t is (idle, setup, cleanup);

    signal seen  : phase_t := idle;
    signal count : natural := 0;

begin

    main: process is

        -- Named exactly as the literals of phase_t
        procedure setup is
        begin
            seen  <= phase_t'(setup);
            count <= count + 1;
            wait for 1 ns;
        end procedure;

        procedure cleanup is
        begin
            seen  <= phase_t'(cleanup);
            wait for 1 ns;
        end procedure;

        -- And a function of one of those names as well, which a statement
        -- must not reach either
        impure function cleanup return boolean is
        begin
            return seen = phase_t'(cleanup);
        end function;

        variable p : phase_t;

    begin
        -- A statement is the procedure
        setup;
        assert seen = phase_t'(setup) report "setup did not run" severity failure;
        assert count = 1 report "setup ran the wrong number of times" severity failure;

        -- An expression is the literal
        p := setup;
        assert p = phase_t'(setup) report "literal not read as a literal"
            severity failure;
        assert phase_t'pos (setup) = 1 report "wrong position" severity failure;

        cleanup;
        assert seen = phase_t'(cleanup) report "cleanup did not run" severity failure;

        -- And where a boolean is wanted, the function
        assert cleanup report "function not read as a function" severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
        wait;
    end process;

end architecture;
