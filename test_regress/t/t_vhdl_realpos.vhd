-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_realpos is
end entity;

architecture a of t_vhdl_realpos is
    type colour_t is (red, green, blue);

    -- A conversion to REAL of a value the back end has still to settle:
    -- 'POS gives a position of the type's own width, and how wide that is
    -- is not known where the conversion is written
    function weigh (s : string) return real is
        variable t : real := 0.0;
    begin
        for i in s'range loop
            t := t + real(character'pos(s(i)));
        end loop;
        return t;
    end function;

    function shade (c : colour_t) return real is
    begin
        return real(colour_t'pos(c)) * 1.5;
    end function;
begin
    process
    begin
        assert weigh("abc") = 294.0
            report "weigh = " & real'image(weigh("abc")) severity failure;
        assert weigh("") = 0.0 report "empty" severity failure;

        assert shade(red) = 0.0 report "red" severity failure;
        assert shade(green) = 1.5 report "green" severity failure;
        assert shade(blue) = 3.0 report "blue" severity failure;

        -- And back again
        assert integer(shade(blue)) = 3 report "round trip" severity failure;

        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
