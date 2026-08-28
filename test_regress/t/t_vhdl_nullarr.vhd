-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_nullarr is
end entity;

architecture test of t_vhdl_nullarr is

    -- A null array has no elements at all (5.3.2.1)
    type t_rec is record
        x : integer;
        y : bit_vector(1 to 0);
        z : integer;
    end record;

    signal s : t_rec := (42, "", 55);

begin

    process is
        variable v : t_rec := (666, "", 12);
    begin
        assert s.x = 42 report "x" severity failure;
        assert s.z = 55 report "z" severity failure;
        assert s.y'length = 0 report "length" severity failure;
        assert s.y = "" report "empty" severity failure;
        assert v.x = 666 report "vx" severity failure;
        assert v.z = 12 report "vz" severity failure;
        v.z := 13;
        s.y <= "";
        wait for 1 ns;
        assert v.z = 13 report "vz2" severity failure;
        assert s.x = 42 report "x2" severity failure;
        assert s.z = 55 report "z2" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
