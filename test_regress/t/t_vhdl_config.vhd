-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An entity with two architectures, and a configuration that says which one
-- to build. A configuration declaration applies to the hierarchy elaborated
-- through it (3.4), so it is the configuration that is named as the top
-- here; elaborating the entity would take the architecture analyzed last,
-- which is what nvc 1.22.1, GHDL 7.0.0-dev and Questa 2025.1 all do.

entity t_vhdl_config is
  port (o : out bit_vector(3 downto 0));
end entity;

architecture chosen_one of t_vhdl_config is
begin
  o <= "0001";
end architecture;

architecture chosen_two of t_vhdl_config is
begin
  o <= "0010";
end architecture;

-- Analyzed last, so this is the one elaborating the entity itself takes
architecture chosen_three of t_vhdl_config is
begin
  o <= "0100";
end architecture;

configuration pick of t_vhdl_config is
  for chosen_one
  end for;
end configuration;
