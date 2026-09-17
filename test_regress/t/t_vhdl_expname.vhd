-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A labelled construct is a declarative region, and its label is a prefix for
-- what it declares (8.3): "p.v" is the variable v of the process labelled p.
-- The entity's name and the architecture's name reach what either declares.

entity t_vhdl_expname is
  generic (G : integer := 9);
end entity;

use std.env.all;

architecture tb of t_vhdl_expname is
  signal s : integer := 7;
begin

  b1: block is
    signal s : integer := 5;
  begin
    g1: for i in 0 to 0 generate
      signal s : integer := 3;
    begin
      p1: process is
        variable s : integer := 1;
      begin
        -- Each region's own s, named through the region that declares it
        assert p1.s = 1 report "p1.s" severity failure;
        assert g1.s = 3 report "g1.s" severity failure;
        assert b1.s = 5 report "b1.s" severity failure;
        assert tb.s = 7 report "tb.s" severity failure;
        -- The entity's name reaches what the entity itself declares. It
        -- does not reach what the architecture declares: 8.3 allows that and
        -- Questa 2025.2 takes it, but nvc 1.22.1 and GHDL 7.0.0-dev both
        -- refuse it, and so does this.
        assert t_vhdl_expname.G = 9 report "entity.G" severity failure;
        -- A simple name still denotes the innermost declaration
        s := s + 1;
        assert s = 2 report "plain s" severity failure;
        assert p1.s = 2 report "p1.s again" severity failure;
        wait;
      end process p1;
    end generate;
  end block;

  main: process is
  begin
    wait for 1 ns;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
