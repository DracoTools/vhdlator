-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A binding indication may name a configuration rather than an entity, and
-- that configuration made both choices already: which entity, and which
-- architecture. It also carries bindings of its own for whatever that
-- architecture instantiates, and those are part of what the instance is
-- built from (3.4, 11.7). Naming only the entity takes the first choice
-- and throws the rest away, leaving the instances inside bound by default.
--
-- Here an inverter is built out of a nand whose second input is tied high,
-- reached through two configurations one naming the other.

entity t_vhdl_cfgchain_leaf is
  port (a, b : in bit; y : out bit);
end entity;

architecture rtl of t_vhdl_cfgchain_leaf is
begin
  y <= a nand b;
end architecture;

configuration t_vhdl_cfgchain_leafcfg of t_vhdl_cfgchain_leaf is
  for rtl
  end for;
end configuration;

entity t_vhdl_cfgchain_mid is
  port (gin : in bit; gout : out bit);
end entity;

architecture rtl of t_vhdl_cfgchain_mid is
  component nandbox
    port (i1, i2 : in bit; o1 : out bit);
  end component;
  signal high : bit := '1';
begin
  nc : nandbox port map (gin, high, gout);
end architecture;

configuration t_vhdl_cfgchain_midcfg of t_vhdl_cfgchain_mid is
  for rtl
    for nc : nandbox
      -- Names a configuration, whose own choice of architecture stands
      use configuration work.t_vhdl_cfgchain_leafcfg port map (i1, i2, o1);
    end for;
  end for;
end configuration;

entity t_vhdl_cfgchain is
end entity;

architecture rtl of t_vhdl_cfgchain is
  component inv
    port (entra : in bit; sale : out bit);
  end component;
  signal si, so : bit;
begin
  inverter : inv port map (si, so);

  ck : process
  begin
    si <= '0';
    wait for 5 ns;
    assert so = '1' report "did not invert a low" severity failure;
    si <= '1';
    wait for 5 ns;
    assert so = '0' report "did not invert a high" severity failure;
    report "*-* All Finished *-*";
    std.env.stop;
    wait;
  end process;
end architecture;

-- The top is a configuration, and its binding names another configuration
configuration t_vhdl_cfgchain_cfg of t_vhdl_cfgchain is
  for rtl
    for inverter : inv
      use configuration work.t_vhdl_cfgchain_midcfg port map (entra, sale);
    end for;
  end for;
end configuration;
