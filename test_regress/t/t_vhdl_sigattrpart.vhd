-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

entity t_vhdl_sigattrpart is
end entity;

architecture test of t_vhdl_sigattrpart is
    type rec_t is record
        f : bit;
        g : bit;
    end record;

    signal v : bit_vector(1 to 3) := "000";
    signal r : rec_t := ('0', '0');
begin

    -- 14.7.3.4: each scalar subelement of a composite signal is a signal of
    -- its own, so an event on one of them is not an event on the others
    main : process is
    begin
        assert v(1)'delayed = '0' report "delayed at the start" severity failure;

        v(2) <= '1';
        wait for 0 ns;
        assert v(2)'event report "event on the element written" severity failure;
        assert not v(1)'event report "event on an element left alone" severity failure;
        assert not v(3)'event report "event on the last element" severity failure;
        assert v(2)'last_value = '0' report "last value of the element" severity failure;
        assert v(2)'delayed = '0' report "delayed still one behind" severity failure;

        wait for 0 ns;
        assert not v(2)'event report "event outlasting its delta" severity failure;

        r.g <= '1';
        wait for 0 ns;
        assert r.g'event report "event on the element assigned" severity failure;
        assert not r.f'event report "event on the other element" severity failure;
        assert r.g'last_value = '0' report "last value of a record element"
            severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
    end process;
end architecture;
