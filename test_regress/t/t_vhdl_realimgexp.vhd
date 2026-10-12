-- DESCRIPTION: Verilator: VHDL REAL image with an exponent, and "%a"
--
-- An image with an exponent writes it as a VHDL literal does (15.5.2):
-- "2.5e7", "1.0e-15", never C's "2.5e+07". The mantissa always has its
-- point. Whether the value folds or not, the image is the same, and it
-- reads back through 'VALUE. TO_STRING with "%a" is C's hexadecimal form,
-- which nvc, GHDL and Questa all print alike, and TO_STRING with DIGITS 0
-- is TO_STRING of the value alone (5.7).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_realimgexp is
end entity;

architecture test of t_vhdl_realimgexp is
    procedure check (got, want : string) is
    begin
        assert got = want report "got " & got & ", want " & want severity failure;
    end procedure;
begin
    process is
        variable r : real;
    begin
        r := 25.0e6;
        check(real'image(r), "2.5e7");
        check(real'image(25.0e6), "2.5e7");
        check(to_string(r), "2.5e7");
        r := 10.0e6;
        check(real'image(r), "1.0e7");
        r := 1.0e-15;
        check(real'image(r), "1.0e-15");
        check(real'image(1.0e-15), "1.0e-15");
        r := -2.5e-7;
        check(real'image(r), "-2.5e-7");
        r := 3.6e18;
        check(real'image(r), "3.6e18");
        r := 1.0e300;
        check(real'image(r), "1.0e300");
        r := 3600.0;
        check(real'image(r), "3.6e3");
        -- Without an exponent, as before
        r := 0.75;
        check(real'image(r), "0.75");
        r := 123456789.0;
        check(real'image(r), "123456789.0");
        -- Each reads back as the value it came from
        for i in -20 to 20 loop
            r := 1.5 * 10.0 ** i;
            assert real'value(real'image(r)) = r
                report "round trip of " & real'image(r) severity failure;
        end loop;
        -- DIGITS of 0 is the image itself
        r := 25.0e6;
        check(to_string(r, 0), "2.5e7");
        r := 1.25;
        check(to_string(r, 0), "1.25");
        -- Hexadecimal
        r := 1.5;
        check(to_string(r, "%a"), "0x1.8p+0");
        check(to_string(r, "%.3a"), "0x1.800p+0");
        check(to_string(r, "%A"), "0X1.8P+0");
        r := -0.1;
        check(to_string(r, "%a"), "-0x1.999999999999ap-4");
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
