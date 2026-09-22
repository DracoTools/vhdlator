-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A port of mode linkage is there to be connected and nothing more: the
-- design neither reads it nor drives it (6.5.6.3). So an input of the
-- enclosing entity may be the actual of one, and the association adds no
-- source to it -- which is what emitting the linkage formal as an output
-- made illegal.

entity t_vhdl_linkage_sub is
  port (c1 : linkage integer;
        c2 : linkage integer);
end entity;

architecture rtl of t_vhdl_linkage_sub is
begin
  quiet : process
  begin
    wait;
  end process;
end architecture;

entity t_vhdl_linkage_mid is
  port (p1 : in  integer;
        p2 : out integer;
        px : in  integer;
        py : in  integer);
end entity;

architecture rtl of t_vhdl_linkage_mid is
  component sub_comp
    port (c1 : linkage integer;
          c2 : linkage integer);
  end component;
  for L : sub_comp use entity work.t_vhdl_linkage_sub(rtl);
begin
  -- Inputs of this entity as the actuals of linkage formals, which is
  -- what emitting the formal as an output made illegal. Each is the actual
  -- of one formal: an association is a source, so two would be two sources
  -- on an unresolved signal, which nvc 1.22.1 refuses outright.
  L : sub_comp port map (px, py);
  p2 <= p1 + 1;
end architecture;

entity t_vhdl_linkage is
end entity;

architecture rtl of t_vhdl_linkage is
  signal a : integer := 7;
  signal b : integer;
  signal x, y : integer;
begin
  dut : entity work.t_vhdl_linkage_mid(rtl)
    port map (p1 => a, p2 => b, px => x, py => y);

  check : process
  begin
    wait for 10 ns;
    assert b = 8 report "bad value past a linkage association" severity failure;
    report "*-* All Finished *-*";
    std.env.stop;
    wait;
  end process;
end architecture;
