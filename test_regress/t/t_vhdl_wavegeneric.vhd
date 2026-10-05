-- DESCRIPTION: Verilator: VHDL delayed concurrent assignment of a generic
-- to a port that several instances drive
--
-- "o <= value after 0.5 ns" with value a generic reads nothing that can
-- change, so it runs once and schedules its one transaction (11.6). Two
-- instances drive one resolved bus, one of them with 'Z's, which makes the
-- port a tristate; the value arrives when the delay says, resolved (GHDL
-- gna bug03).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_wavegeneric_cell is
  generic (value : std_logic_vector(3 downto 0) := (others => 'Z'));
  port (o : out std_logic_vector(3 downto 0));
end entity;

architecture a of t_vhdl_wavegeneric_cell is
begin
  o <= value after 0.5 ns;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_wavegeneric is
end entity;

architecture a of t_vhdl_wavegeneric is
  signal s : std_logic_vector(3 downto 0);
begin
  u1 : entity work.t_vhdl_wavegeneric_cell generic map (value => "0101") port map (s);
  u2 : entity work.t_vhdl_wavegeneric_cell generic map (value => "ZZZZ") port map (s);

  process is
  begin
    wait for 0.4 ns;
    assert s /= "0101" severity failure;
    wait for 0.6 ns;
    assert s = "0101" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
