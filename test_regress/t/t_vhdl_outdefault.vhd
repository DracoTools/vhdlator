-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A signal whose one source is an instance's output starts at that output's
-- default, the entity's, which the component declaration does not repeat:
-- every signal's first value is worked out from its sources before any
-- process runs (14.7.5.2), so a process reading it at time zero sees '0'
-- and not the signal's own 'U' (GHDL gna issue30)

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_outdefault_gen is
  port (rstn : out std_logic := '0'; level : out integer := 7);
end entity;

architecture a of t_vhdl_outdefault_gen is
begin
  process
  begin
    rstn <= '1' after 5 ns;
    level <= 9 after 5 ns;
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_outdefault is
end entity;

architecture a of t_vhdl_outdefault is
  component t_vhdl_outdefault_gen is
    port (rstn : out std_logic; level : out integer);
  end component;
  signal rstn : std_logic;
  signal level : integer;
  signal first : boolean := true;
  signal seen : std_logic := '1';
  signal seen_level : integer := -1;
begin
  u : t_vhdl_outdefault_gen port map (rstn, level);

  -- Runs once before time advances, reading what initialization settled
  process (rstn)
  begin
    if first then
      first <= false;
      seen <= rstn;
      seen_level <= level;
    end if;
  end process;

  process
  begin
    wait for 1 ns;
    assert seen = '0' report "rstn started at " & std_logic'image(seen) severity failure;
    assert seen_level = 7 report "level started at " & integer'image(seen_level)
      severity failure;
    wait for 10 ns;
    assert rstn = '1' and level = 9 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
