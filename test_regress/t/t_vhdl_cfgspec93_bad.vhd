-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- VHDL-2008 lets a configuration specification be closed by "end for";
-- before it, one is not (7.3.1). References: nvc 1.22.1, GHDL 7.0.0-dev,
-- Questa 2025.2.

entity leaf is
  port (i : in bit);
end entity;

architecture a of leaf is
begin
end architecture;

entity t is
end entity;

architecture a of t is
  signal s : bit;
  component leaf is
    port (i : in bit);
  end component;
  for u : leaf
    use entity work.leaf;
  end for;
begin
  u : leaf port map (s);
end architecture;
