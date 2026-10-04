-- DESCRIPTION: Verilator: VHDL conversion between arrays of INTEGER and REAL
--
-- INTEGER_VECTOR and REAL_VECTOR are closely related: each is an array of
-- one dimension whose element type is closely related to the other's, so a
-- conversion between them converts each element as a conversion of it
-- would, a REAL rounded to the nearest INTEGER (9.3.6; nvc conv10).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_convrealvec is
end entity;

architecture test of t_vhdl_convrealvec is
    type rarr is array (3 downto 0) of real;
    signal s : integer_vector(0 to 1) := (10, -3);
    signal t : real_vector(0 to 1);
begin
    t <= real_vector(s);

    process is
        variable i : integer_vector(1 to 3);
        variable r : real_vector(1 to 3);
        variable d : rarr;
        variable i4 : integer_vector(1 to 4) := (4, 6, -8, 9);
    begin
        i := (1, 2, 3);
        r := real_vector(i);
        assert r = (1.0, 2.0, 3.0) severity failure;
        r := (4.2, 5.9, -7.5);
        i := integer_vector(r);
        assert i = (4, 6, -8) report "rounded" severity failure;
        d := rarr(i4);
        assert d = (4.0, 6.0, -8.0, 9.0) report "to a descending array" severity failure;
        assert d(3) = 4.0 and d(0) = 9.0 report "by position" severity failure;
        wait for 1 ns;
        assert t = (10.0, -3.0) report "signal" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
