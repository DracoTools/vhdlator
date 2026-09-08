-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A qualified aggregate as the actual of a call whose body reads its argument
-- more than once. MAXIMUM over one array reads every element of it, and an
-- aggregate has no places to select from until it is one -- so the value is
-- worked out once and kept, the same thing done wherever a value that cannot
-- be read twice has to be.

entity t_vhdl_qualagg is
end entity;

architecture a of t_vhdl_qualagg is

    type real_vec is array (natural range <>) of real;

begin

    main: process is
        variable n : integer;
        variable r : real;
        variable v : integer_vector (1 to 4) := (8, 1, 6, 3);
    begin
        -- The aggregate, written straight into the call
        n := maximum (integer_vector'(4, 9, 2));
        assert n = 9 report "max " & integer'image (n) severity failure;
        n := minimum (integer_vector'(4, 9, 2));
        assert n = 2 report "min " & integer'image (n) severity failure;

        -- A single element, and one where the extreme is at each end
        n := maximum (integer_vector'(1 => 7));
        assert n = 7 report "one" severity failure;
        n := maximum (integer_vector'(9, 1, 2));
        assert n = 9 report "max at left" severity failure;
        n := minimum (integer_vector'(3, 1, 9));
        assert n = 1 report "min in middle" severity failure;

        -- An array of a floating point element, which compares differently
        r := maximum (real_vec'(1.5, -2.25, 0.75));
        assert r = 1.5 report "real max" severity failure;

        -- And an ordinary object, which always worked and must keep working
        n := maximum (v);
        assert n = 8 report "object" severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
        wait;
    end process;

end architecture;
