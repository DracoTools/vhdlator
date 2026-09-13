-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A configuration's binding indication carries a generic map of its own, and
-- its actuals may name the component's own generics (3.4.2, 7.3.2.2): the
-- component's local generic holds what the instance gave it, or the default
-- the component declared, and the entity's formal takes what that makes.

entity t_vhdl_cfggen_sub is
  generic ( g1 : integer;
            g2 : integer );
  port ( ok : out boolean );
end entity;

architecture rtl of t_vhdl_cfggen_sub is
begin
  ok <= (g1 = 4) and (g2 = 7);
end architecture;

entity t_vhdl_cfggen is
end entity;

use std.env.all;

architecture tb of t_vhdl_cfggen is
  component socket is
    generic ( g1 : integer := 2;
              g2 : integer := 5 );
    port ( ok : out boolean );
  end component;
  signal done : boolean;
begin

  -- The instance gives g2 and leaves g1 to the component's default
  inst : socket generic map (g2 => 5) port map (ok => done);

  process is
  begin
    wait for 1 ns;
    assert done report "the configuration's generics did not arrive" severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;

configuration t_vhdl_cfggen_cfg of t_vhdl_cfggen is
  for tb
    for inst : socket use entity work.t_vhdl_cfggen_sub(rtl)
      -- Both actuals name the component's own generics
      generic map (g1 => g1 + g1, g2 => g2 + 2);
    end for;
  end for;
end configuration;
