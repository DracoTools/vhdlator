-- DESCRIPTION: Verilator: VHDL concatenation of what a call returns with an aggregate
--
-- An array whose elements do not pack is concatenated element by element,
-- and what a function returns is worked out once for that, not once per
-- element.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_catcall is
end entity;

architecture test of t_vhdl_catcall is
    type int_array is array (natural range <>) of integer;

    function pad (a : in int_array) return int_array is
    begin
        return (0, 0) & a;
    end function;
begin

    process is
        variable x : int_array(0 to 3);
        variable w : int_array(0 to 5);
        variable d : integer := 0;
        variable calls : natural := 0;

        impure function get_array (base : in integer) return int_array is
        begin
            calls := calls + 1;
            return (base + 1, base + 2, base + 3);
        end function;
    begin
        x := get_array(d) & (0 => 4);
        assert x = (1, 2, 3, 4) severity failure;
        x := get_array(d) & (6 => 8);
        assert x = (1, 2, 3, 8) severity failure;
        x := (6 => 9) & get_array(d);
        assert x = (9, 1, 2, 3) severity failure;
        d := 10;
        w := get_array(0) & get_array(d);
        assert w = (1, 2, 3, 11, 12, 13) severity failure;
        -- Each call was made once
        assert calls = 5 report integer'image(calls) severity failure;
        x := pad((1, 2));
        assert x = (0, 0, 1, 2) severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
