-- DESCRIPTION: Verilator: VHDL aggregate whose elements are slices of one
-- element
--
-- "((0 => 1), (0 => 200), (0 => 300))" of an array of INTEGERs: an
-- aggregate cannot be an INTEGER, so each inner one is a value of the
-- outer aggregate's own type, a run of one element (9.3.3.3). nvc 1.22.1
-- and Questa 2025.2 accept it; GHDL 7.0.0-dev refuses ("type integer is
-- not composite") (nvc parse issue541).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_aggslice1 is
end entity;

architecture test of t_vhdl_aggslice1 is
    type t_arr is array (natural range <>) of integer;

    procedure check (constant values : in t_arr; constant name : in string := "") is
    begin
        assert values'length = 3 report name & " length" severity failure;
        assert values(values'left) = 1 report name & " first" severity failure;
        assert values(values'right) = 300 report name & " last" severity failure;
    end procedure;

    constant one : t_arr(0 to 0) := (0 => 7);
begin
    process is
        variable v : t_arr(0 to 2);
        variable w : t_arr(1 to 4);
    begin
        v := ((0 => 1), (0 => 200), (0 => 300));
        assert v = (1, 200, 300) report "v" severity failure;
        w := (5, one, (3 => 8), 9);
        assert w = (5, 7, 8, 9) report "w" severity failure;
        check(((0 => 1), (0 => 200), (0 => 300)), name => "call");
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
