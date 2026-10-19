-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A slice whose bounds a generic settles is checked as the model starts,
-- whether or not the statement it is in ever runs: tmp(47 downto 32) of a
-- tmp(31 downto 0) is an error of elaboration where nvc and GHDL stop (GHDL
-- gna bug16782), though the branch is one only a width of 8 takes.

entity t_vhdl_elabslice_bad_sub is
  port (a : in bit_vector);
end entity;

architecture behav of t_vhdl_elabslice_bad_sub is
  constant W : natural := a'length;
  signal tmp : bit_vector(31 downto 0);
begin
  process (a) begin
    case W is
      when 8 => tmp(3 * W - 1 downto 2 * W) <= a;
      when 16 => tmp(2 * W - 1 downto W) <= a;
      when others => null;
    end case;
  end process;
end architecture;

entity t_vhdl_elabslice_bad is
end entity;

architecture behav of t_vhdl_elabslice_bad is
  signal a : bit_vector(15 downto 0);
begin
  dut : entity work.t_vhdl_elabslice_bad_sub port map (a => a);

  process begin
    wait for 1 ns;
    report "not stopped" severity failure;
    wait;
  end process;
end architecture;
