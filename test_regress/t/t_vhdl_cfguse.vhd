-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A block configuration may carry use clauses of its own, and what they make
-- visible is a declarative item of that configuration's region: the binding
-- indications written after them name it (3.4.2, 12.4). Here the generic map
-- of a component configuration takes its actual from a package the block
-- configuration used, and from the component's own generic.

package cfgusepkg is
  constant wanted : integer := 42;
end package;

entity t_vhdl_cfguse_sub is
  generic (got : integer := 0;
           also : integer := 0);
end entity;

architecture rtl of t_vhdl_cfguse_sub is
begin
  check : process
  begin
    assert got = 42 report "bad generic from the use clause" severity failure;
    assert also = 7 report "bad generic from the component" severity failure;
    report "*-* All Finished *-*";
    std.env.stop;
    wait;
  end process;
end architecture;

entity t_vhdl_cfguse is
end entity;

architecture rtl of t_vhdl_cfguse is
  component socket
    generic (mine : integer := 7);
  end component;
begin
  inst : component socket;
end architecture;

configuration t_vhdl_cfguse_cfg of t_vhdl_cfguse is
  for rtl
    use work.cfgusepkg.wanted;   -- a declarative item of this region
    for inst : socket
      use entity work.t_vhdl_cfguse_sub(rtl)
        generic map (got => wanted, also => mine);
    end for;
  end for;
end configuration;
