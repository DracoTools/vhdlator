-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- IEEE's fixed-point package: conversions in and out, and RESIZE.
--
-- A subprogram written against an unconstrained array is compiled once per
-- call site, and the copy's result has a length the declaration never had --
-- but the copies are made in a round after the analysis that settles types, so
-- at lowering the call's own type still says nothing, and whoever asks it is
-- told nothing. TO_REAL writes "arg_int := To_X01(cleanvec(arg))" and TO_SULV
-- returns "STD_ULOGIC_VECTOR(intermediate_result)" sized from arg'length, so
-- both hand a value with a width to something told there was none. The model
-- built but did not compile, which is worse than an error because it passes
-- the check a lint run makes.
--
-- The package also asks for a null slice: "invec (right_index - 1 downto
-- arglow)" where the two are adjacent names no elements at all (5.2.1).
--
-- Arithmetic is not covered here. "p + q" is held back by a separate defect:
-- the length of a user-defined operator's result is settled before the copy
-- made for the call site folds its own bounds, so it comes out as long as an
-- operand instead of one element longer.

library ieee;
use ieee.std_logic_1164.all;
use ieee.fixed_pkg.all;

entity t_vhdl_fixedpkg is
end entity;

architecture a of t_vhdl_fixedpkg is
begin

    main: process is
        variable p : sfixed (8 downto -8);
        variable u : ufixed (8 downto -8);
        variable n : sfixed (3 downto -4);

        -- Every value here is exact in eight fractional bits, so these are
        -- equalities and not tolerances
        procedure check_real (got : real; want : real; what : string) is
        begin
            assert got = want report what severity failure;
        end procedure;
    begin
        -- TO_REAL of an sfixed, which is the chain the defect was found in
        p := to_sfixed (1.5, 8, -8);
        check_real (to_real (p), 1.5, "sfixed positive");
        p := to_sfixed (-3.125, 8, -8);
        check_real (to_real (p), -3.125, "sfixed negative");
        p := to_sfixed (0.0625, 8, -8);
        check_real (to_real (p), 0.0625, "sfixed small");
        p := to_sfixed (100.25, 8, -8);
        check_real (to_real (p), 100.25, "sfixed large");

        -- And of a ufixed, whose conversions are declared separately
        u := to_ufixed (225.5625, 8, -8);
        check_real (to_real (u), 225.5625, "ufixed");
        u := to_ufixed (0.00390625, 8, -8);
        check_real (to_real (u), 0.00390625, "ufixed smallest");

        -- RESIZE between two widths, which is where the null slice is asked
        -- for: narrowing drops fractional bits it cannot keep
        p := to_sfixed (2.25, 8, -8);
        n := resize (p, n);
        check_real (to_real (n), 2.25, "resize narrower");
        p := resize (n, p);
        check_real (to_real (p), 2.25, "resize wider");

        -- Between the two families
        u := to_ufixed (7.5, 8, -8);
        p := resize (to_sfixed (u), p);
        check_real (to_real (p), 7.5, "ufixed to sfixed");

        report "*-* All Finished *-*";
        std.env.stop;
        wait;
    end process;

end architecture;
