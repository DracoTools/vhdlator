-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

entity t_vhdl_force is
end entity;

architecture test of t_vhdl_force is
    signal s : bit_vector(1 to 3);
    signal n : natural;
begin

    -- An ordinary driver, which goes on driving while the signal is forced
    n <= 7 after 1 ns;

    main : process is
        variable k : integer;
    begin
        s <= "101";
        wait for 1 ns;
        assert s = "101" report "the driver" severity failure;
        assert n = 7 report "the other driver" severity failure;

        -- 10.5.2.1: a force is a signal assignment, so it takes effect at
        -- the end of the delta like any other
        s <= force "010";
        assert s = "101" report "not yet" severity failure;
        wait for 0 ns;
        assert s = "010" report "forced" severity failure;
        assert s'event report "a force is a transaction" severity failure;

        -- What the drivers say while the force is in charge is kept, and
        -- shows again when the signal is released
        k := 3;
        s(k) <= '0';
        wait for 0 ns;
        assert s = "010" report "the force still holds" severity failure;

        s <= release;
        wait for 0 ns;
        assert s = "100" report "released to what the drivers held: "
            & to_string(s) severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
    end process;
end architecture;
