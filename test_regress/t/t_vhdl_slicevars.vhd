-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- Slices whose two ends are different variables. Deciding whether the ends
-- are a fixed distance apart matched the terms of each end by shape, and two
-- names had the same shape whatever they named: "v(a downto b)" was taken as
-- one element wide, and "v1(v2 to v3) := v4(v2 to v3)" as a part select with
-- ends that are not constants (VESTS tc1140).
entity t_vhdl_slicevars is
end entity;

architecture test of t_vhdl_slicevars is
begin
    process
        type enum1 is (m1, m2, m3, m4, m5);
        type abase is array (enum1 range <>) of boolean;
        subtype a1 is abase(enum1 range m1 to m5);
        variable v1 : a1 := (others => false);
        variable v4 : a1 := (true, false, true, true, false);
        variable v2, v3 : enum1;
        variable v : bit_vector(7 downto 0) := "10110110";
        variable w : bit_vector(7 downto 0) := (others => '0');
        variable a, b, c, d : integer := 0;
        variable n : natural;
    begin
        -- The ends are never written, so each is m1: one element
        v1(v2 to v3) := v4(v2 to v3);
        assert v1 = (true, false, false, false, false) report "tc1140's shape" severity failure;
        v2 := m2;
        v3 := m4;
        v1(v2 to v3) := v4(v2 to v3);
        assert v1 = (true, false, true, true, false) report "three elements" severity failure;

        a := 5;
        b := 2;
        -- Nothing else here says how long it is
        assert v(a downto b) = "1101" report "four bits" severity failure;
        n := v(a downto b)'length;
        assert n = 4 report "and four long" severity failure;
        c := 6;
        d := 3;
        w(c downto d) := v(a downto b);
        assert w = "01101000" report "into another such slice" severity failure;

        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
