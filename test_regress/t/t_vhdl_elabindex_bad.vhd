-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An index that an instance's generics settle is checked as the model starts
-- (GHDL gna ticket9): "a(n + 3)" of a port a the actual makes n long.

entity t_vhdl_elabindex_bad_sub is
  port (a : in bit_vector);
end entity;

architecture behav of t_vhdl_elabindex_bad_sub is
  constant n : natural := a'length;
  signal tmp : bit;
begin
  tmp <= a(n + 3);
end architecture;

entity t_vhdl_elabindex_bad is
end entity;

architecture behav of t_vhdl_elabindex_bad is
  signal a : bit_vector(15 downto 0);
begin
  sub : entity work.t_vhdl_elabindex_bad_sub port map (a => a);
  process begin
    wait for 1 ns;
    report "not stopped" severity failure;
    wait;
  end process;
end architecture;
