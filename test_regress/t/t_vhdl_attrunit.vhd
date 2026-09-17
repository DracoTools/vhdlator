-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A design unit is given a user-defined attribute in its own declarative
-- part, and reads it back through its own name: "pkg'a" inside the package,
-- "arch'a" inside the architecture, "ent'a" for the entity the architecture
-- belongs to (7.2, 16.2).

package t_vhdl_attrunit_pkg is
  attribute a1 : integer;
  attribute a2 : string;
  attribute a1 of t_vhdl_attrunit_pkg : package is 3;
  -- The value read back is a static expression, so a later declaration in
  -- this same package may be written in terms of it
  constant c1 : integer := t_vhdl_attrunit_pkg'a1;
  attribute a2 of t_vhdl_attrunit_pkg : package is "pack";
  constant c2 : string := t_vhdl_attrunit_pkg'a2;
end package;

use work.t_vhdl_attrunit_pkg.all;
use std.env.all;

entity t_vhdl_attrunit is
  attribute a1 of t_vhdl_attrunit : entity is 11;
end entity;

architecture tb of t_vhdl_attrunit is
  attribute a1 of tb : architecture is 22;
  attribute a2 of tb : architecture is "arch";
  signal s : integer := 0;
  attribute a1 of s : signal is 33;
begin

  main: process is
  begin
    assert c1 = 3 report "c1" severity failure;
    assert c2 = "pack" report "c2" severity failure;
    assert work.t_vhdl_attrunit_pkg'a1 = 3 report "pkg'a1" severity failure;
    assert t_vhdl_attrunit'a1 = 11 report "entity'a1" severity failure;
    assert tb'a1 = 22 report "arch'a1" severity failure;
    assert tb'a2 = "arch" report "arch'a2" severity failure;
    -- An object given the same attribute keeps its own value
    assert s'a1 = 33 report "s'a1" severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
