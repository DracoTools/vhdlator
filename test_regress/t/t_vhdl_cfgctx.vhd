-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A block configuration's declarative region is nested in the region of the
-- block it configures (12.1), so everything visible where the architecture
-- ends is visible in the configuration. That is not only what the
-- architecture declares but also what its entity's context clause made
-- visible, which no use clause of the configuration itself repeats here.

package cfgctxpkg is
  constant wanted : integer := 42;
end package;

entity t_vhdl_cfgctx_sub is
  generic (got : integer := 0);
end entity;

architecture rtl of t_vhdl_cfgctx_sub is
begin
  check : process
  begin
    assert got = 55 report "bad generic from the inherited context" severity failure;
    report "*-* All Finished *-*";
    std.env.stop;
    wait;
  end process;
end architecture;

use work.cfgctxpkg.all;  -- the entity's context clause, inherited by rtl
entity t_vhdl_cfgctx is
end entity;

architecture rtl of t_vhdl_cfgctx is
  constant extra : integer := 13;
  component socket
  end component;
begin
  inst : component socket;
end architecture;

configuration t_vhdl_cfgctx_cfg of t_vhdl_cfgctx is
  for rtl
    for inst : socket
      -- wanted comes from the entity's context clause, extra from the
      -- architecture's own declarative part
      use entity work.t_vhdl_cfgctx_sub(rtl) generic map (got => wanted + extra);
    end for;
  end for;
end configuration;
