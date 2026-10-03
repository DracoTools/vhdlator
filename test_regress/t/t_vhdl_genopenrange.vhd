-- DESCRIPTION: Verilator: VHDL generate over the range of an open port (11.8)
--
-- "for k in i'range generate" over a port whose bounds, and so whose
-- direction, each instance takes from its actual: the generate makes one
-- block per index whichever way the range runs.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_genopenrange_sub is
    port ( i : in bit_vector; o : out bit_vector; n : out natural );
end entity;

architecture test of t_vhdl_genopenrange_sub is
begin
    g: for k in i'range generate
        o(k) <= not i(k);
    end generate;

    process (i) is
        variable count : natural;
    begin
        count := 0;
        for k in i'range loop
            count := count + 1;
        end loop;
        n <= count;
    end process;
end architecture;

entity t_vhdl_genopenrange is
end entity;

architecture test of t_vhdl_genopenrange is
    signal a, b : bit_vector(1 to 4) := "1010";
    signal c, d : bit_vector(7 downto 5) := "110";
    signal na, nc : natural;
begin
    u1: entity work.t_vhdl_genopenrange_sub port map ( a, b, na );
    u2: entity work.t_vhdl_genopenrange_sub port map ( c, d, nc );

    process is
    begin
        wait for 1 ns;
        assert b = "0101" severity failure;
        assert d = "001" severity failure;
        assert na = 4 and nc = 3 severity failure;
        a <= "0011";
        c <= "100";
        wait for 1 ns;
        assert b = "1100" severity failure;
        assert d = "011" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
