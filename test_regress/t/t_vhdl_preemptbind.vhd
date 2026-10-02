-- DESCRIPTION: Verilator: VHDL delayed assignment in an entity bound twice
--
-- An entity that schedules a signal ahead of time, instantiated directly and
-- again through a configuration that binds a component inside it: each
-- lowering of it has drivers of its own.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_preemptbind_leaf is
end entity;

architecture a of t_vhdl_preemptbind_leaf is
begin
end architecture;

entity t_vhdl_preemptbind_dly is
    generic (g : boolean);
    port (p1 : in bit;
          p2 : out bit);
end entity;

architecture a of t_vhdl_preemptbind_dly is
    component zippy
    end component;
begin
    u1 : zippy;
    p2 <= p1 after 10 ns;
end architecture;

configuration t_vhdl_preemptbind_cfg of t_vhdl_preemptbind_dly is
    for a
        for u1 : zippy use entity work.t_vhdl_preemptbind_leaf;
        end for;
    end for;
end configuration;

entity t_vhdl_preemptbind is
end entity;

architecture test of t_vhdl_preemptbind is
    signal s1, s2, s3 : bit;
begin
    -- Once as the entity itself, once as the configuration of it
    c1 : entity work.t_vhdl_preemptbind_dly generic map (true) port map (s1, s2);
    c2 : configuration work.t_vhdl_preemptbind_cfg generic map (false) port map (s2, s3);

    process is
    begin
        s1 <= '1';
        wait for 15 ns;
        assert s2 = '1' and s3 = '0' severity failure;
        s1 <= '0';              -- a pulse of 4 ns, shorter than the delay
        wait for 4 ns;
        s1 <= '1';
        wait for 3 ns;
        assert s2 = '1' and s3 = '1' severity failure;
        wait for 20 ns;
        assert s2 = '1' and s3 = '1' severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
