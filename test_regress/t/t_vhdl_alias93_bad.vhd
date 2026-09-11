-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Before VHDL-2008 an object alias names no object of a multidimensional
-- array type (VHDL-93 4.3.3.1). References: nvc 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

entity t_vhdl_alias93_bad is
end entity;

architecture a of t_vhdl_alias93_bad is
  type grid is array (0 to 1, 0 to 1) of bit;
  type row is array (0 to 1) of bit;
  constant g : grid := (others => (others => '0'));
  constant r : row := (others => '1');
  -- Legal
  alias r2 : row is r;
  -- Illegal
  alias g2 : grid is g;
begin
end architecture;
