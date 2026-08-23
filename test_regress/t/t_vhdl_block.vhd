-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A block statement with generics and ports of its own. A block is not a
-- module: it is elaborated where it stands, so each generic is a constant
-- and each port a signal wired to its actual.

library ieee;
use ieee.std_logic_1164.all;
use std.env.all;

entity t_vhdl_block is
end entity;

architecture sim of t_vhdl_block is
  signal x, y, z : integer;
begin

  scaled : block is
    generic (factor : integer);
    generic map (factor => 5);
    port (i : in integer; o : out integer);
    port map (i => x, o => y);
  begin
    o <= i * factor;
  end block;

  -- A plain block is just a declarative region
  plain : block is
    signal t : integer;
  begin
    t <= y + 1;
    z <= t;
  end block;

  stim : process is
  begin
    x <= 1;
    wait for 1 ns;
    assert y = 5 report "block port or generic is wrong" severity failure;
    assert z = 6 report "plain block is wrong" severity failure;
    x <= 10;
    wait for 1 ns;
    assert y = 50 report "block did not follow its input" severity failure;
    assert z = 51 report "plain block did not follow" severity failure;
    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
