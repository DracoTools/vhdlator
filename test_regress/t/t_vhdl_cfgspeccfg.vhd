-- DESCRIPTION: Verilator: VHDL configuration specification that uses a configuration
--
-- "for all : c use configuration work.cfg;" written in an architecture binds
-- the instances to what that configuration chose (7.3.2.1), with the
-- bindings the configuration itself carries.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_cfgspeccfg_la is port (o : out integer); end entity;
architecture a of t_vhdl_cfgspeccfg_la is begin o <= 1; end architecture;
entity t_vhdl_cfgspeccfg_lb is port (o : out integer); end entity;
architecture a of t_vhdl_cfgspeccfg_lb is begin o <= 2; end architecture;

entity t_vhdl_cfgspeccfg_mid is
    generic (g : integer := 10);
    port (o : out integer);
end entity;
architecture a of t_vhdl_cfgspeccfg_mid is
    component leaf port (o : out integer); end component;
    signal v : integer;
begin
    u1 : leaf port map (v);
    o <= v + g;
end architecture;

configuration t_vhdl_cfgspeccfg_ca of t_vhdl_cfgspeccfg_mid is
    for a
        for u1 : leaf use entity work.t_vhdl_cfgspeccfg_la; end for;
    end for;
end configuration;
configuration t_vhdl_cfgspeccfg_cb of t_vhdl_cfgspeccfg_mid is
    for a
        for u1 : leaf use entity work.t_vhdl_cfgspeccfg_lb; end for;
    end for;
end configuration;

entity t_vhdl_cfgspeccfg is end entity;
architecture test of t_vhdl_cfgspeccfg is
    component c port (o : out integer); end component;
    component d port (o : out integer); end component;
    for all : c use configuration work.t_vhdl_cfgspeccfg_ca;
    for d1 : d use configuration work.t_vhdl_cfgspeccfg_cb generic map (g => 20);
    for others : d use configuration work.t_vhdl_cfgspeccfg_ca generic map (g => 30);
    signal r1, r2, r3, r4 : integer;
begin
    c1 : c port map (r1);
    c2 : c port map (r2);
    d1 : d port map (r3);
    d2 : d port map (r4);
    process is
    begin
        wait for 1 ns;
        assert r1 = 11 and r2 = 11 report "all" severity failure;
        assert r3 = 22 report "by label" severity failure;
        assert r4 = 31 report "others" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
