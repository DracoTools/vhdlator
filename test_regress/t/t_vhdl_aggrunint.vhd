-- DESCRIPTION: Verilator: VHDL-2008 aggregate with runs of elements, elements that do not pack
--
-- A value of the aggregate's own type stands for as many elements as it is
-- long (9.3.3.3): by position, against a range, as a name, a qualified
-- aggregate, a bare aggregate, or what a function returns.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_aggrunint is
end entity;

architecture test of t_vhdl_aggrunint is
begin

    main : process is
        variable x : integer_vector(1 to 4);
        variable y, z : integer_vector(1 to 2);
        variable calls : natural := 0;

        impure function pair (base : integer) return integer_vector is
        begin
            calls := calls + 1;
            return (base, base + 1);
        end function;
    begin
        x := (integer_vector'(1, 2), integer_vector'(3, 4));
        assert x = (1, 2, 3, 4) severity failure;
        y := (5, 6);
        z := (7, 8);
        x := (y, z);
        assert x = (5, 6, 7, 8) severity failure;
        x := (1 to 2 => z, 3 to 4 => integer_vector'(1, 2));
        assert x = (7, 8, 1, 2) severity failure;
        x := (4 downto 1 => x);
        assert x = (7, 8, 1, 2) severity failure;
        x := (y, 9, 8);
        assert x = (5, 6, 9, 8) severity failure;
        x := (9, 8, (1, 2));
        assert x = (9, 8, 1, 2) severity failure;
        x := (3, y, 4);
        assert x = (3, 5, 6, 4) severity failure;
        x := (pair(20), pair(30));
        assert x = (20, 21, 30, 31) severity failure;
        assert calls = 2 severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
