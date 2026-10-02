-- DESCRIPTION: Verilator: VHDL TEXTIO field width, justification, and SREAD
--
-- WRITE puts its item in a field of at least FIELD characters, against the
-- right of it or the left as JUSTIFIED says (16.4); DIGITS gives a real its
-- places after the point.  The optional parameters are named as often as
-- they are placed.  SREAD takes the next word of the line.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

use std.textio.all;

entity t_vhdl_textiofield is
end entity;

architecture test of t_vhdl_textiofield is
begin

    process is
        variable l : line;
        variable w : natural := 6;
        variable j : side := left;
        variable s : string(1 to 5);
        variable n : natural;

        procedure expect (want : in string) is
        begin
            assert l.all = want report "got [" & l.all & "] want [" & want & "]"
                severity failure;
            deallocate(l);
        end procedure;
    begin
        write(l, integer'(42), right, 6);
        expect("    42");
        write(l, integer'(42), left, 6);
        expect("42    ");
        write(l, string'("ab"), right, 5);
        expect("   ab");
        write(l, string'("ab"), left, 5);
        expect("ab   ");
        write(l, bit_vector'("1010"), right, 6);
        expect("  1010");
        write(l, true, field => 7);
        expect("   true");
        write(l, 'x', left, 3);
        expect("x  ");
        hwrite(l, bit_vector'("1010"), field => 5);
        expect("    A");
        hwrite(l, bit_vector'(X"abc"), field => 5, justified => left);
        expect("ABC  ");
        owrite(l, bit_vector'(O"77"), right, 4);
        expect("  77");
        write(l, 1.5, right, 10, 2);
        expect("      1.50");
        write(l, 2.25, digits => 3);
        expect("2.250");
        write(l, 10 ns, right, 8);
        expect("   10 ns");

        -- An item longer than its field is written whole
        write(l, integer'(12345), right, 3);
        expect("12345");
        write(l, string'("abcdef"), left, 2);
        expect("abcdef");

        -- A width and a side that only running decides
        write(l, integer'(7), j, w);
        expect("7     ");
        j := right;
        w := 3;
        write(l, integer'(7), j, w);
        expect("  7");
        w := 0;
        write(l, integer'(7), j, w);
        expect("7");

        -- Items follow one another in the line, each in its own field
        write(l, integer'(1), right, 3);
        write(l, string'("x"), left, 3);
        write(l, integer'(2), right, 3);
        expect("  1x    2");

        -- SREAD: leading blanks are skipped, the word ends at a blank
        write(l, string'("  one  three end"));
        sread(l, s, n);
        assert n = 3 and s(1 to 3) = "one" severity failure;
        sread(l, s, n);
        assert n = 5 and s = "three" severity failure;
        sread(l, s, n);
        assert n = 3 and s(1 to 3) = "end" severity failure;
        sread(l, s, n);
        assert n = 0 severity failure;
        deallocate(l);

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
