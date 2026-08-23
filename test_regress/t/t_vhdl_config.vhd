-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An entity with two architectures, and a configuration that says which one
-- to build. Without the configuration the default binding takes the one
-- analyzed last, so this test only passes if the configuration is honoured.

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

configuration pick of t_vhdl_config is
  for chosen_one
  end for;
end configuration;
