-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A block configuration names what it configures: within an architecture's,
-- a block or a generate statement of that architecture; within a component
-- configuration, the architecture the binding names.
-- References: nvc 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

entity t_vhdl_cfgnames_leaf is
end entity;

architecture rtl of t_vhdl_cfgnames_leaf is
begin
  inner : block
  begin
  end block;
end architecture;

entity t_vhdl_cfgnames_bad is
end entity;

architecture a of t_vhdl_cfgnames_bad is
  component leaf
  end component;
begin
  blk : block
  begin
    u : leaf;
  end block;
  gen : for i in 0 to 1 generate
  end generate;
end architecture;

configuration t_vhdl_cfgnames_cfg of t_vhdl_cfgnames_bad is
  for a
    -- Legal
    for gen end for;
    for blk
      for u : leaf use entity work.t_vhdl_cfgnames_leaf(rtl);
        for rtl
          for inner end for;
        end for;
      end for;
    end for;
    -- Illegal
    for nosuch end for;
  end for;
end configuration;

configuration t_vhdl_cfgnames_cfg2 of t_vhdl_cfgnames_bad is
  for a
    for blk
      for u : leaf use entity work.t_vhdl_cfgnames_leaf(rtl);
        -- Illegal
        for other end for;
      end for;
    end for;
  end for;
end configuration;

configuration t_vhdl_cfgnames_cfg3 of t_vhdl_cfgnames_bad is
  for a
    for blk
      for u : leaf use entity work.t_vhdl_cfgnames_leaf(rtl);
        for rtl
          -- Illegal
          for missing end for;
        end for;
      end for;
    end for;
  end for;
end configuration;
