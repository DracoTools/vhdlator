-- DESCRIPTION: Verilator: VHDL port of a record whose open elements are inside a nested record
--
-- A record port left open, whose open elements are two records down -- an
-- array of vectors and a vector -- given its bounds by two instances of
-- different sizes. Each record is a structure of a class whose parameters
-- are its open bounds, and a nested one takes its bounds from its parent's.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_recportnest_pkg is
  type slv_array is array (natural range <>) of std_logic_vector;
  type leaf_t is record
    words : slv_array;
    tag : std_logic_vector;
  end record;
  type mid_t is record
    leaf : leaf_t;
    n : natural;
  end record;
  type top_t is record
    m : mid_t;
    ok : boolean;
  end record;
end package;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_recportnest_pkg.all;
entity t_vhdl_recportnest_sub is
  port (x : in top_t; y : out top_t);
end entity;
architecture a of t_vhdl_recportnest_sub is
begin
  process (x)
    variable v : top_t(m(leaf(words(x.m.leaf.words'range)(x.m.leaf.words(x.m.leaf.words'low)'range),
                               tag(x.m.leaf.tag'range))));
  begin
    v := x;
    for i in v.m.leaf.words'range loop
      v.m.leaf.words(i) := not v.m.leaf.words(i);
    end loop;
    v.m.n := x.m.n + x.m.leaf.words'length;
    v.ok := not x.ok;
    y <= v;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_recportnest_pkg.all;
entity t_vhdl_recportnest is end entity;
architecture a of t_vhdl_recportnest is
  subtype t8 is top_t(m(leaf(words(0 to 2)(7 downto 0), tag(3 downto 0))));
  subtype t4 is top_t(m(leaf(words(1 to 2)(3 downto 0), tag(1 downto 0))));
  signal s8, r8 : t8;
  signal s4, r4 : t4;
begin
  u8 : entity work.t_vhdl_recportnest_sub port map (x => s8, y => r8);
  u4 : entity work.t_vhdl_recportnest_sub port map (x => s4, y => r4);
  process
  begin
    s8 <= (m => (leaf => (words => (x"0f", x"f0", x"55"), tag => x"a"), n => 1), ok => false);
    s4 <= (m => (leaf => (words => (x"3", x"c"), tag => "10"), n => 5), ok => true);
    wait for 1 ns;
    assert r8.m.leaf.words(0) = x"f0" severity failure;
    assert r8.m.leaf.words(2) = x"aa" severity failure;
    assert r8.m.leaf.tag = x"a" severity failure;
    assert r8.m.n = 4 severity failure;
    assert r8.ok severity failure;
    assert r4.m.leaf.words(1) = x"c" severity failure;
    assert r4.m.leaf.words(2) = x"3" severity failure;
    assert r4.m.n = 7 severity failure;
    assert not r4.ok severity failure;
    assert r4.m.leaf.words'left = 1 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
