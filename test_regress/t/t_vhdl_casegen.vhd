-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A case generate elaborates the one alternative whose choices cover the
-- selector (11.8). The choices may be a list, a range or "others", the
-- alternatives may be labelled, and each carries a body of its own.

use std.env.all;

entity t_vhdl_casegen is
end entity;

architecture tb of t_vhdl_casegen is

  constant WHICH : integer := 4;
  constant NAMED : integer := 1;

  type mode_t is (slow, fast, off);
  constant SPEED : mode_t := fast;

  signal a, b, c : integer := 0;

begin

  -- The selector falls in a range choice
  g1 : case WHICH generate
    when 1 | 2 => a <= 12;
    when 3 to 5 => a <= 35;
    when others => a <= 99;
  end generate;

  -- An alternative with a label and a declarative part of its own
  g2 : case NAMED generate
    when one : 1 =>
      constant K : integer := 7;
    begin
      b <= K;
    when rest : others =>
      b <= 0;
  end generate;

  -- The selector may be an enumeration
  g3 : case SPEED generate
    when slow => c <= 1;
    when fast => c <= 2;
    when off => c <= 3;
  end generate;

  main : process is
  begin
    wait for 1 ns;
    assert a = 35 report "a=" & integer'image(a) severity failure;
    assert b = 7 report "b=" & integer'image(b) severity failure;
    assert c = 2 report "c=" & integer'image(c) severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
