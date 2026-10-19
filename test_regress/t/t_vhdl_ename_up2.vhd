-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Each caret of an external name's relative pathname climbs one region
-- (8.7): out of the unit's own to the unit instantiating it, and out of that
-- to the one instantiating it in turn.

entity t_vhdl_ename_up2_leaf is
end entity;

architecture behav of t_vhdl_ename_up2_leaf is
begin
  process begin
    wait for 1 ns;
    -- Out of the leaf, out of inner: mid's architecture
    assert <<signal ^.^.mid_sig : natural>> = 7 report "two up" severity failure;
    -- And out of mid: the top's
    assert <<signal ^.^.^.top_sig : natural>> = 5 report "three up" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
    wait;
  end process;
end architecture;

entity t_vhdl_ename_up2_inner is
end entity;

architecture behav of t_vhdl_ename_up2_inner is
begin
  leaf : entity work.t_vhdl_ename_up2_leaf;
end architecture;

entity t_vhdl_ename_up2_mid is
end entity;

architecture behav of t_vhdl_ename_up2_mid is
  signal mid_sig : natural := 7;
begin
  inner : entity work.t_vhdl_ename_up2_inner;
end architecture;

entity t_vhdl_ename_up2 is
end entity;

architecture behav of t_vhdl_ename_up2 is
  signal top_sig : natural := 5;
begin
  mid : entity work.t_vhdl_ename_up2_mid;
end architecture;
