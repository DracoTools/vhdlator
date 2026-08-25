-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An external name reaches an object elsewhere in the design hierarchy
-- (8.7), by a pathname relative to where it is written or absolute from the
-- root. A forced signal keeps the value it was given until it is released
-- (10.5.2.1).

entity t_vhdl_extname_sub is
  port (o : out integer);
end entity;

architecture rtl of t_vhdl_extname_sub is
  signal inner : integer := 7;
begin
  o <= inner;
end architecture;

use std.env.all;

entity t_vhdl_extname is
end entity;

architecture tb of t_vhdl_extname is
  signal top_sig : integer := 1;
  signal from_sub : integer;
begin

  uut : entity work.t_vhdl_extname_sub port map (o => from_sub);

  process is
  begin
    wait for 1 ns;

    -- A relative pathname, from where it is written
    assert <<signal uut.inner : integer>> = 7
      report "relative pathname" severity failure;

    -- The same object, named absolutely from the root of the design
    assert <<signal .t_vhdl_extname.uut.inner : integer>> = 7
      report "absolute pathname" severity failure;

    -- Writing through one drives the object it names
    <<signal uut.inner : integer>> <= 9;
    wait for 1 ns;
    assert from_sub = 9 report "writing through an external name" severity failure;

    -- A force wins over what the design drives, until it is released
    <<signal uut.inner : integer>> <= force 42;
    wait for 1 ns;
    assert from_sub = 42 report "force" severity failure;
    <<signal uut.inner : integer>> <= 3;
    wait for 1 ns;
    assert from_sub = 42 report "a force is not overwritten" severity failure;
    -- Released, the signal is driven again by whatever drives it
    <<signal uut.inner : integer>> <= release;
    wait for 1 ns;
    <<signal uut.inner : integer>> <= 5;
    wait for 1 ns;
    assert from_sub = 5 report "release" severity failure;

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
