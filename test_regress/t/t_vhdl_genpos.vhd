-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity sub is
    -- An unconstrained generic, whose bounds a hidden generic carries
    generic (g : bit_vector;
             n : integer := 0);
    port (o : out integer);
end entity;

architecture rtl of sub is
begin
    o <= g'length + n;
end architecture;

entity t_vhdl_genpos is
end entity;

architecture test of t_vhdl_genpos is
    type pair is record
        a, b : integer;
    end record;

    signal o1, o2, o3 : integer;
begin

    -- Positional, so it must reach g and not the hidden one
    u1 : entity work.sub generic map ("101") port map (o1);
    u2 : entity work.sub generic map ("1010", 7) port map (o2);
    -- The parts of one generic, named one at a time (6.5.7.1)
    u3 : entity work.sub generic map (g(0) => '1', g(1) => '0', g(2) => '1')
        port map (o3);

    process is
    begin
        wait for 1 ns;
        assert o1 = 3 report "o1" severity failure;
        assert o2 = 11 report "o2" severity failure;
        assert o3 = 3 report "o3" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
