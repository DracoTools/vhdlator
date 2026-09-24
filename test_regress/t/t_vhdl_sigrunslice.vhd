-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A signal assignment in a process to a slice whose ends only running
-- settles: one scheduled assignment per place. Left to the assignment, the
-- slice was a part select whose ends are not constants (GHDL's issue2620).
entity t_vhdl_sigrunslice is
end entity;

architecture test of t_vhdl_sigrunslice is
    signal dec : bit_vector(7 downto 0) := (others => '1');
    signal s : bit_vector(0 to 7) := (others => '0');
    signal d : bit_vector(0 to 7) := (others => '0');
begin
    process
        variable v : bit_vector(0 to 7) := "10110010";
        variable a, b : natural;
    begin
        for i in 0 to 7 loop
            dec <= (others => '1');
            dec(i downto 0) <= (others => '0');
            wait for 1 ns;
            for k in 0 to 7 loop
                assert (dec(k) = '0') = (k <= i) report "cleared" severity failure;
            end loop;
        end loop;

        a := 2;
        b := 5;
        s(a to b) <= v(a + 1 to b + 1);
        wait for 1 ns;
        assert s = "00100100" report "from another such slice" severity failure;

        d(b to b + 2) <= v(0 to 2) after 3 ns;
        wait for 2 ns;
        assert d = "00000000" report "not yet" severity failure;
        wait for 2 ns;
        assert d = "00000101" report "after its delay" severity failure;

        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
