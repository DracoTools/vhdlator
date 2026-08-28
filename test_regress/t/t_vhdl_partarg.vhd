-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_partarg is
end entity;

architecture test of t_vhdl_partarg is

    type rec is record
        x, y : integer;
    end record;

    function total (r : rec) return integer is
    begin
        return r.x + r.y;
    end function;

    function width (v : bit_vector) return natural is
    begin
        return v'length;
    end function;

begin

    process is
        variable a, b : integer;
    begin
        -- The parts of one parameter, named one at a time (6.5.7.1)
        assert total(r.x => 1, r.y => 2) = 3 report "literal" severity failure;
        a := 4;
        b := 5;
        assert total(r.x => a, r.y => b) = 9 report "variable" severity failure;
        assert width(v(0) => '1', v(1) => '0') = 2 report "indexed" severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
