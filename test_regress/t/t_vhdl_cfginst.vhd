-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A configuration is a design unit of its own, and an instance may name one
-- (11.7). Two configurations of one entity say different things, so each is
-- built from its own bindings -- and a binding's port map may be written
-- positionally, where the Nth association names the Nth port of the entity
-- rather than of the component (7.3.2.2).

entity t_vhdl_cfginst_sub is
  port (a, b : in integer; q : out boolean);
end entity;
architecture rtl of t_vhdl_cfginst_sub is
begin
  q <= a > b;
end architecture;

entity t_vhdl_cfginst_mid is
  port (res : out boolean);
end entity;

architecture sim of t_vhdl_cfginst_mid is
  signal x, y : integer := 0;
  component cmp is
    port (a, b : in integer; q : out boolean);
  end component;
begin
  dut : component cmp port map (a => x, b => y, q => res);
  x <= 4;
  y <= 2;
end architecture;

-- One binds the ports straight through; the other crosses them, and writes
-- its map positionally
configuration t_vhdl_cfginst_gt of t_vhdl_cfginst_mid is
  for sim
    for dut : cmp use entity work.t_vhdl_cfginst_sub(rtl)
      port map (a => a, b => b, q => q);
    end for;
  end for;
end configuration;

configuration t_vhdl_cfginst_lt of t_vhdl_cfginst_mid is
  for sim
    for dut : cmp use entity work.t_vhdl_cfginst_sub(rtl)
      port map (b, a, q);
    end for;
  end for;
end configuration;

use std.env.all;

entity t_vhdl_cfginst is
end entity;

architecture tb of t_vhdl_cfginst is
  signal rgt, rlt : boolean;
begin
  -- Both configurations of the one entity, each keeping its own bindings
  u1 : configuration work.t_vhdl_cfginst_gt port map (res => rgt);
  u2 : configuration work.t_vhdl_cfginst_lt port map (res => rlt);

  main : process is
  begin
    wait for 1 ns;
    assert rgt report "4 > 2 should hold" severity failure;
    assert not rlt report "2 > 4 should not hold" severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;
end architecture;
