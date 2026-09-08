-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- IMAGE of a floating point value, and TO_STRING of one, use the smallest
-- number of digits that represents the value accurately (16.4) -- the shortest
-- decimal that reads back as the same value. Verilog's %g is six significant
-- digits, which is right for Verilog and wrong here: 225.5625 printed as
-- 225.562 is a different number.
--
-- Whether the value folds used to decide which of two rules applied, so the
-- same value printed two ways depending on where it was written. Both now
-- follow the one rule.
--
-- Note two deliberate divergences from nvc 1.22.1, both shared with GHDL
-- 7.0.0-dev: a whole number keeps its point, since a VHDL real literal must
-- have one (15.5) and the image has to read back through 'VALUE; and one
-- third takes sixteen digits, because fifteen -- which nvc prints -- does not
-- read back as the same value.

entity t_vhdl_realimg is
end entity;

architecture a of t_vhdl_realimg is

    -- Not a constant: a value the compiler folds takes the other path, and
    -- the point of this test is that the two agree
    signal held : real := 225.5625;

begin

    main: process is
        variable r : real;
    begin
        -- The value that found this: six digits would drop the last one
        r := 225.5625;
        assert real'image (r) = "225.5625"
            report "runtime: " & real'image (r) severity failure;
        assert real'image (225.5625) = "225.5625"
            report "folded: " & real'image (225.5625) severity failure;
        assert to_string (held) = "225.5625"
            report "to_string: " & to_string (held) severity failure;

        -- A whole number keeps its point, so that it is a real literal
        r := 3.0;
        assert real'image (r) = "3.0" report "whole: " & real'image (r) severity failure;
        r := 0.0;
        assert real'image (r) = "0.0" report "zero: " & real'image (r) severity failure;
        r := -4.0;
        assert real'image (r) = "-4.0" report "negative whole: " & real'image (r)
            severity failure;

        -- Exact binary fractions of several lengths
        r := 0.5;
        assert real'image (r) = "0.5" report "half: " & real'image (r) severity failure;
        r := 0.00390625;
        assert real'image (r) = "0.00390625" report "small: " & real'image (r)
            severity failure;
        r := 1234567.75;
        assert real'image (r) = "1234567.75" report "large: " & real'image (r)
            severity failure;

        -- And one that needs every digit it is given
        r := 1.0 / 3.0;
        assert real'image (r) = "0.3333333333333333"
            report "third: " & real'image (r) severity failure;

        -- The image reads back as the value it came from, which is what the
        -- decimal point is there for
        r := 3.0;
        assert real'value (real'image (r)) = 3.0
            report "no round trip" severity failure;
        r := 225.5625;
        assert real'value (real'image (r)) = 225.5625
            report "no round trip for a fraction" severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
        wait;
    end process;

end architecture;
