-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A configuration declaration says which entity each component instance is
-- built from, and may give the generics itself (3.4.3, 7.3.2). The
-- configuration is also what the design is elaborated from, so it is what
-- names the top unit.

entity t_vhdl_configbind_sub is
  generic (g : integer);
  port (o : out integer);
end entity;

architecture rtl of t_vhdl_configbind_sub is
begin
  o <= g;
end architecture;

use std.env.all;

entity t_vhdl_configbind_top is
end entity;

architecture arch of t_vhdl_configbind_top is

  component comp is
    port (o : out integer);
  end component;

  signal a : integer := 0;
  signal b : integer := 0;

begin

  u1 : component comp port map (o => a);
  u2 : component comp port map (o => b);

  main : process is
  begin
    wait for 1 ns;
    assert a = 7 report "u1 got " & integer'image(a) severity failure;
    assert b = 9 report "u2 got " & integer'image(b) severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;

configuration t_vhdl_configbind of t_vhdl_configbind_top is
  for arch
    for u1 : comp
      use entity work.t_vhdl_configbind_sub(rtl)
        generic map (g => 7);
    end for;
    for u2 : comp
      use entity work.t_vhdl_configbind_sub(rtl)
        generic map (g => 9);
    end for;
  end for;
end configuration;
