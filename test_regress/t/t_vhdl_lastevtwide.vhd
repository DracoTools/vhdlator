-- DESCRIPTION: Verilator: VHDL 'LAST_EVENT of wide signals and their delays
--
-- A signal nothing has changed has had no event, so S'LAST_EVENT is
-- TIME'HIGH (16.2.4) whatever its width; and the signal S'DELAYED(T) makes,
-- of a whole record or of one of its elements, has a history of its own.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_lastevtwide is
end entity;

architecture test of t_vhdl_lastevtwide is
    type r_t is record
        a : bit;
        b : bit_vector(3 downto 0);
    end record;
    signal v : bit_vector(7 downto 0);
    signal r : r_t;
    signal i : integer;
    signal q : r_t;
begin

    process is
    begin
        wait for 0 ns;
        wait for 1 ns;
        -- Nothing has happened to any of them
        assert v'last_event = time'high severity failure;
        assert r'last_event = time'high severity failure;
        assert i'last_event = time'high severity failure;
        assert v'delayed(2 ns)'last_event = time'high severity failure;
        assert q'delayed(2 ns)'last_event = time'high severity failure;

        q.a <= '1';
        v <= x"0f";
        wait for 1 ns;
        assert q'last_event = 1 ns severity failure;
        assert v'last_event = 1 ns severity failure;
        assert q.a'delayed(2 ns)'last_event = time'high severity failure;
        assert q'delayed(2 ns)'last_event = time'high severity failure;
        wait for 2 ns;
        -- The delayed copies changed 1 ns ago
        assert q.a'delayed(2 ns)'last_event = 1 ns severity failure;
        assert q'delayed(2 ns)'last_event = 1 ns severity failure;
        assert q.b'delayed(2 ns)'last_event = time'high severity failure;
        assert v'delayed(2 ns)'last_event = 1 ns severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
