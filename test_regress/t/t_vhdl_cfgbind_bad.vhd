-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A component configuration binds the component to an entity its library
-- holds (7.3.2.2). References: nvc 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

entity comp is
  port (s : bit);
end entity;

architecture behav of comp is
begin
end architecture;

entity t is
end entity;

architecture behav of t is
  component comp is
    port (s : bit);
  end component;
  signal s : bit;
begin
  c1 : comp port map (s);
  c2 : comp port map (s);
end architecture;

configuration conf of t is
  for behav
    for c1 : comp
      use entity work.comp(behav);
    end for;
    for c2 : comp
      use entity work.compx(behav);  -- Bad
    end for;
  end for;
end configuration;
