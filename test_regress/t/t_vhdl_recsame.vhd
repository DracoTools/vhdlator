-- DESCRIPTION: Verilator: VHDL objects of one record subtype written twice, element left open
--
-- A record whose element is an array of records, itself with an element
-- of an unconstrained array type: two objects each constrain it the same
-- way, and a value of one is assigned to the other (5.3.3).  Each starts
-- with the default of its elements (6.4.2.3).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_recsame is
end entity;

architecture test of t_vhdl_recsame is
    type rec1_t is record
        x : integer_vector;
    end record;

    type rec1_array is array (natural range <>) of rec1_t;

    type rec2_t is record
        a : rec1_array;
    end record;
begin

    p1 : process is
        variable v : rec2_t(a(1 to 2)(x(1 to 3)));
        constant k : rec2_t(a(1 to 2)(x(1 to 3))) := (a => (others => (x => (100, 200, 300))));
        variable w : rec2_t(a(1 to 2)(x(1 to 3)));
    begin
        assert v.a(1).x(1) = integer'left severity failure;
        assert v.a(2).x(3) = integer'left severity failure;
        v.a(1).x := (1, 2, 3);
        wait for 1 ns;
        assert v.a(1).x(2) = 2 severity failure;

        v := (a => k.a);
        v.a(1).x(1) := 999;
        wait for 1 ns;
        assert k.a(1).x(1) = 100 severity failure;
        assert v.a(1).x(1) = 999 and v.a(2).x(3) = 300 severity failure;

        w := v;
        assert w = v severity failure;
        w.a(2) := k.a(1);
        assert w.a(2).x(1) = 100 severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
