-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A configuration specification names a component and instances of it in
-- its own region, each configured once, and binds them to an entity.
-- References: nvc 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

entity t_vhdl_cfgspec_leaf is
end entity;

architecture a of t_vhdl_cfgspec_leaf is
begin
end architecture;

package t_vhdl_cfgspec_pkg is
end package;

entity t_vhdl_cfgspec_bad is
end entity;

architecture a of t_vhdl_cfgspec_bad is
  component c1 is
  end component;
  component c2 is
  end component;
  -- Legal
  for i1 : c1 use entity work.t_vhdl_cfgspec_leaf;
  for i7 : c1 use open;
  for all : c2 use entity work.t_vhdl_cfgspec_leaf;
  -- Illegal
  for i3 : c2 use entity work.t_vhdl_cfgspec_leaf;
  for i3 : t_vhdl_cfgspec_bad use entity work.t_vhdl_cfgspec_leaf;
  for nosuch : c1 use entity work.t_vhdl_cfgspec_leaf;
  for i2 : c1 use entity work.t_vhdl_cfgspec_leaf;
  for i1 : c1 use entity work.t_vhdl_cfgspec_leaf;
  for i5 : c1 use entity work.t_vhdl_cfgspec_leaf;
  for i6 : c1 use entity work.t_vhdl_cfgspec_pkg;
  for all : c2 use entity work.t_vhdl_cfgspec_leaf;
begin
  i1 : component c1;
  i2 : entity work.t_vhdl_cfgspec_leaf;
  i3 : component c1;
  i4 : component c2;
  b1 : block
  begin
    i5 : component c1;
  end block;
  i6 : component c1;
  i7 : component c1;
end architecture;
