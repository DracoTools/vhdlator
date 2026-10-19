-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An aggregate for an array whose element subtype is open, its elements
-- constants of different lengths (9.3.3.3): an error once elaborated, here
-- where the constant's value folds (GHDL gna issue1723).

entity t_vhdl_constagglen_bad is
end entity;

architecture behav of t_vhdl_constagglen_bad is
  type my_vector is array (natural range <>) of integer;
  type my_vector_vector is array (natural range <>) of my_vector;
  constant c_1 : my_vector(1 downto 0) := (others => 1);
  constant c_2 : my_vector(2 downto 0) := (others => 2);
  constant c_foo : my_vector_vector(1 downto 0) := (0 => c_1, 1 => c_2);
begin
  process begin
    wait for 1 ns;
    report "not stopped" severity failure;
    wait;
  end process;
end architecture;
