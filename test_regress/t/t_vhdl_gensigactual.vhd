-- DESCRIPTION: Verilator: VHDL generic given a signal, which is its initial value
--
-- An actual of a generic is read as the design is elaborated, before the
-- signal has changed, so the generic is the signal's initial value whatever
-- the signal is given later (6.5.6.2).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_gensigactual_foo is
    generic (a : natural; b : bit_vector(3 downto 0));
    port (sum : out natural);
end entity;

architecture rtl of t_vhdl_gensigactual_foo is
begin
    sum <= a + 1;
    process is
    begin
        assert a = 5 severity failure;
        assert b = "1010" severity failure;
        wait;
    end process;
end architecture;

entity t_vhdl_gensigactual is
end entity;

architecture tb of t_vhdl_gensigactual is
    signal a_actual : natural := 5;
    signal b_actual : bit_vector(3 downto 0) := "1010";
    signal sum : natural;
begin
    foo_inst : entity work.t_vhdl_gensigactual_foo
        generic map (a => a_actual, b => b_actual)
        port map (sum => sum);

    process is
    begin
        a_actual <= 9;
        b_actual <= "0000";
        wait for 1 ns;
        assert sum = 6 severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
