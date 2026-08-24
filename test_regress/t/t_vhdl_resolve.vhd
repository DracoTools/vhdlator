-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A signal driven from more than one place is resolved (6.5.2). Each driver
-- contributes its own value, and a driver holding 'Z' contributes nothing.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.env.all;

entity t_vhdl_resolve is
end entity;

architecture sim of t_vhdl_resolve is
  signal one   : std_logic;
  signal wide  : std_logic_vector(3 downto 0);
  signal en0   : std_logic := '0';
  signal en1   : std_logic := '0';
  signal fromp : std_logic := 'Z';

  -- A pull-up: weak, so anything forcing beats it
  signal pulled : std_logic;
begin

  pulled <= 'H';
  pulled <= '0' when en0 = '1' else 'Z';


  -- Two concurrent drivers of one signal
  one <= '1' when en0 = '1' else 'Z';
  one <= '0' when en1 = '1' else 'Z';

  wide <= "1010" when en0 = '1' else (others => 'Z');
  wide <= "0101" when en1 = '1' else (others => 'Z');

  -- A process is a driver too, alongside a concurrent assignment
  one <= fromp;

  main : process is
  begin
    wait for 1 ns;

    en0 <= '1';
    wait for 1 ns;
    assert one = '1' report "one driver on" severity failure;
    assert unsigned(wide) = 10 report "vector driver on" severity failure;

    en0 <= '0';
    en1 <= '1';
    wait for 1 ns;
    assert one = '0' report "other driver on" severity failure;
    assert unsigned(wide) = 5 report "other vector driver on" severity failure;

    -- The process driver takes over while the others are off
    en1 <= '0';
    fromp <= '1';
    wait for 1 ns;
    assert one = '1' report "process driver" severity failure;

    fromp <= 'Z';
    wait for 1 ns;

    -- Strength: the pull-up holds the line high until something forces it
    -- The pull-up alone leaves the line weakly high, which is 'H' and not
    -- '1'. Asserting '1' would pass here only because the two collapse
    -- together in a two-state model, and would fail on any other simulator.
    assert pulled = 'H' report "pull-up alone" severity failure;
    en0 <= '1';
    wait for 1 ns;
    assert pulled = '0' report "a forcing low beats a pull-up" severity failure;
    en0 <= '0';
    wait for 1 ns;
    assert pulled = 'H' report "the pull-up returns" severity failure;

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
