-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_value is
end entity;

architecture test of t_vhdl_value is
begin

    process is
        type colour is (red, green, blue);
        type resistance is range 0 to 10000000
            units
                ohm;
                kohm = 1000 ohm;
            end units;
        variable s : string(1 to 10) := (others => 'x');
        variable e : natural := 0;
    begin
        -- Writing part of a STRING (10.6)
        s(1 to 5) := "12345";
        s(7) := 'q';
        assert s = "12345xqxxx" report "part" severity failure;

        while e < s'length and s(e + 1) /= 'x' loop
            e := e + 1;
        end loop;
        assert e = 5 report "scan" severity failure;

        -- The value whose image is the text (16.2.3)
        assert integer'value(s(1 to e)) = 12345 report "slice" severity failure;
        assert integer'value("  -12_3 ") = -123 report "int" severity failure;
        assert boolean'value("TRUE") = true report "bool" severity failure;
        assert character'value("'x' ") = 'x' report "char" severity failure;
        assert colour'value(" Green ") = green report "enum" severity failure;
        assert resistance'value("2 kohm") = 2000 ohm report "phys" severity failure;
        assert real'value("-5e-1") = -0.5 report "real" severity failure;
        assert integer'value(integer'image(integer'low)) = integer'low
            report "roundtrip" severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
