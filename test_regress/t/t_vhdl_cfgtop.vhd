-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A configuration declaration is a design unit of its own, and what it says
-- applies to the hierarchy elaborated through it. A design that elaborates
-- the entity binds its components by the rules of 7.3.3 instead, so an
-- entity of the component's own name is what an instance gets -- not
-- whatever a configuration declaration elsewhere in the file would have
-- said. nvc 1.22.1 and GHDL 7.0.0-dev both answer this way.

entity t_vhdl_cfgtop_copies is
  port (i : in integer; o : out integer);
end entity;

architecture rtl of t_vhdl_cfgtop_copies is
begin
  o <= i;
end architecture;

entity t_vhdl_cfgtop_const is
  port (i : in integer; o : out integer);
end entity;

architecture rtl of t_vhdl_cfgtop_const is
begin
  o <= 99;
end architecture;

-- Two architectures of one entity. Elaborating the entity takes the one
-- analyzed last (7.3.3); only a configuration names the other.
entity t_vhdl_cfgtop_two is
  port (o : out integer);
end entity;

architecture first of t_vhdl_cfgtop_two is
begin
  o <= 1;
end architecture;

architecture last of t_vhdl_cfgtop_two is
begin
  o <= 2;
end architecture;

configuration t_vhdl_cfgtop_two_cfg of t_vhdl_cfgtop_two is
  for first
  end for;
end;

use std.env.all;

entity t_vhdl_cfgtop is
end entity;

architecture tb of t_vhdl_cfgtop is
  -- The component's name is an entity's name, so default binding has an
  -- answer for it
  component t_vhdl_cfgtop_copies is
    port (i : in integer; o : out integer);
  end component;
  signal src : integer := 5;
  signal direct : integer := 0;
  signal gen : integer := 0;
  signal which : integer := 0;
begin

  -- Instantiated directly, so the architecture is the one analyzed last
  two: entity work.t_vhdl_cfgtop_two port map (o => which);

  k: block
  begin
    u1: t_vhdl_cfgtop_copies port map (src, direct);
    g: for n in 0 to 0 generate
      u2: t_vhdl_cfgtop_copies port map (src, gen);
    end generate;
  end block;

  main: process is
  begin
    wait for 1 ns;
    assert direct = 5
      report "the instance took a configuration this elaboration did not go through: "
        & integer'image(direct) severity failure;
    assert gen = 5 report "the generated instance got " & integer'image(gen)
      severity failure;
    assert which = 2
      report "the architecture analyzed last was not taken: " & integer'image(which)
      severity failure;
    report "*-* All Finished *-*" severity note;
    stop(0);
  end process;

end architecture;

-- Never elaborated: the top named is the entity above, so nothing here applies
configuration t_vhdl_cfgtop_cfg of t_vhdl_cfgtop is
  for tb
    for k
      for all: t_vhdl_cfgtop_copies use entity work.t_vhdl_cfgtop_const(rtl);
      end for;
    end for;
  end for;
end;
