-- DESCRIPTION: Verilator: VHDL positional aggregates written through an
-- access value (9.3.3.2, 10.6.2)
--
-- An aggregate of positional values written into what an access value
-- points at, whole or as a slice, gives each place the value at its
-- position; and the bounds of what it points at answer 'LOW and 'HIGH.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_accaggr is
end entity;

architecture test of t_vhdl_accaggr is
    type int_vec is array (integer range <>) of integer;
    type int_vec_ptr is access int_vec;
begin

    process is
        variable p, q, d : int_vec_ptr;
    begin
        p := new int_vec(1 to 10);
        p(1 to 3) := (1, 2, 3);
        assert p(1) = 1 and p(2) = 2 and p(3) = 3 severity failure;
        p.all(4 to 6) := (4, 5, 6);
        assert p.all(4) = 4 and p.all(6) = 6 severity failure;
        assert p'length = 10 severity failure;
        assert p.all'low = 1 severity failure;
        assert p.all'high = 10 severity failure;

        q := new int_vec(0 to 2);
        q.all := (7, 8, 9);
        assert q(0) = 7 and q(1) = 8 and q(2) = 9 severity failure;

        -- Positions count from the left, whatever the direction
        d := new int_vec(5 downto 1);
        d.all := (50, 40, 30, 20, 10);
        assert d(5) = 50 and d(1) = 10 severity failure;
        d(3 downto 2) := (33, 22);
        assert d(3) = 33 and d(2) = 22 and d(4) = 40 severity failure;
        assert d.all'low = 1 and d.all'high = 5 severity failure;

        deallocate(p);
        deallocate(q);
        deallocate(d);
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
