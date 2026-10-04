-- DESCRIPTION: Verilator: VHDL false generate with a signal read one element at a time
--
-- A statement reading one element of a signal waits on that element alone,
-- through a copy of it made for the purpose. Where the signal is declared by
-- an if-generate whose condition is false, there is no signal, and a copy
-- standing in the module named nothing (GHDL gna ticket32: an internal
-- fault). The copy is made beside the statement, inside the generate.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_genfalsesig_sub is
    generic (n : positive);
    port (x : in bit_vector(n - 1 downto 0); y : out bit_vector(n - 1 downto 0));
end entity;

architecture rtl of t_vhdl_genfalsesig_sub is
begin
    y(0) <= x(0);
    gen1 : if n > 1 generate
        signal p : bit_vector(n - 1 downto 1);
    begin
        p(1) <= x(0) and x(1);
        y(1) <= p(1);
        gen2 : if n > 2 generate
            chain : for i in 2 to n - 1 generate
                p(i) <= p(i - 1) and x(i);
                y(i) <= p(i);
            end generate;
        end generate;
    end generate;
end architecture;

entity t_vhdl_genfalsesig is
end entity;

architecture test of t_vhdl_genfalsesig is
    signal x1, y1 : bit_vector(0 downto 0);
    signal x4, y4 : bit_vector(3 downto 0);
begin
    u1 : entity work.t_vhdl_genfalsesig_sub generic map (n => 1) port map (x => x1, y => y1);
    u4 : entity work.t_vhdl_genfalsesig_sub generic map (n => 4) port map (x => x4, y => y4);

    process is
    begin
        x1 <= "1";
        x4 <= "0111";
        wait for 1 ns;
        assert y1 = "1" severity failure;
        assert y4 = "0111" severity failure;
        x4 <= "1111";
        wait for 1 ns;
        assert y4 = "1111" severity failure;
        x4 <= "1101";
        wait for 1 ns;
        assert y4 = "0001" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
