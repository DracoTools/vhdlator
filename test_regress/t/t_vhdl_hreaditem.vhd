-- DESCRIPTION: Verilator: VHDL HREAD and OREAD take one item (16.4)
--
-- Blanks come only before the item, an underscore may stand between two of
-- its digits, and a first digit saying more bits than the value has places
-- for must have those bits zero.  A read that fails says so through GOOD and
-- takes what it read off the line.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

use std.textio.all;

entity t_vhdl_hreaditem is
end entity;

architecture test of t_vhdl_hreaditem is
begin

    process is
        variable l : line;
        variable b : bit_vector(1 to 16);
        variable c : bit_vector(7 downto 0);
        variable b13 : bit_vector(12 downto 0);
        variable b11 : bit_vector(10 downto 0);
        variable o6 : bit_vector(1 to 6);
        variable o5 : bit_vector(4 downto 0);
        variable good : boolean;
    begin
        l := new string'("  1 2 ab23 12_34 dead 12 1__2");
        hread(l, b(1 to 4));
        assert b(1 to 4) = "0001" severity failure;
        hread(l, b(1 to 4));
        assert b(1 to 4) = "0010" severity failure;
        hread(l, b);
        assert b = X"ab23" severity failure;
        hread(l, b);
        assert b = X"1234" severity failure;
        hread(l, b);
        assert b = X"dead" severity failure;
        hread(l, b, good);              -- "12" is two digits short
        assert not good severity failure;
        hread(l, c, good);              -- two underscores together
        assert not good severity failure;
        deallocate(l);

        -- A blank ends the item
        l := new string'(" 1 2");
        hread(l, c, good);
        assert not good severity failure;
        deallocate(l);
        l := new string'("1");
        hread(l, c, good);
        assert not good severity failure;
        assert l.all = "" severity failure;
        deallocate(l);

        -- An underscore after the last digit is not part of the item
        l := new string'(" 1f_ xy");
        hread(l, c, good);
        assert good and c = X"1f" severity failure;
        assert l.all = "_ xy" severity failure;
        deallocate(l);

        -- Thirteen bits from four digits: the first may only be 0 or 1
        l := new string'("0ABC 1ABC 2ABC");
        hread(l, b13, good);
        assert good and b13 = "0101010111100" severity failure;
        hread(l, b13, good);
        assert good and b13 = "1101010111100" severity failure;
        hread(l, b13, good);
        assert not good severity failure;
        deallocate(l);
        l := new string'("821");        -- one bit too many
        hread(l, b11, good);
        assert not good severity failure;
        deallocate(l);
        l := new string'("721");
        hread(l, b11, good);
        assert good and b11 = "11100100001" severity failure;
        deallocate(l);

        l := new string'("  1 2 5_2 37 47");
        oread(l, o6(1 to 3));
        assert o6(1 to 3) = "001" severity failure;
        oread(l, o6(1 to 3));
        assert o6(1 to 3) = "010" severity failure;
        oread(l, o6);
        assert o6 = "101010" severity failure;
        oread(l, o5, good);
        assert good and o5 = "11111" severity failure;
        oread(l, o5, good);
        assert not good severity failure;
        deallocate(l);

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
