-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A generic record an impure function builds travels as data, and a bound
-- written in terms of it becomes a parameter each instance works out. A
-- component that hands part of its own such generic to an instance --
-- "h => m.leaf" -- could not work that instance's bounds out: its generic
-- has no value until it is instantiated in turn, and the port kept a
-- default one bit wide. The same held for a generate condition written in
-- terms of it, which was refused as not constant. VUnit's verification
-- components nest this way (axi_stream_master, axi_stream_monitor).
library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_datagennest_pkg is
  type leaf_t is record
    id : integer;
    len : positive;
  end record;
  type mid_t is record
    id : integer;
    leaf : leaf_t;
  end record;
  type top_t is record
    mid : mid_t;
    extra : natural;
  end record;
  impure function new_leaf(len : positive) return leaf_t;
  impure function new_mid(len : positive) return mid_t;
  impure function new_top(len : positive; extra : natural) return top_t;
end package;

package body t_vhdl_datagennest_pkg is
  type counter_t is protected
    impure function next_id return integer;
  end protected;
  type counter_t is protected body
    variable n : integer := 0;
    impure function next_id return integer is
    begin
      n := n + 1;
      return n;
    end function;
  end protected body;
  shared variable counter : counter_t;

  impure function new_leaf(len : positive) return leaf_t is
  begin
    return (id => counter.next_id, len => len);
  end;
  impure function new_mid(len : positive) return mid_t is
  begin
    return (id => 100, leaf => new_leaf(len));
  end;
  impure function new_top(len : positive; extra : natural) return top_t is
  begin
    return (mid => new_mid(len), extra => extra);
  end;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_datagennest_pkg.all;

entity t_vhdl_datagennest_leaf is
  generic (h : leaf_t);
  port (d : in std_logic_vector(h.len - 1 downto 0); o : out integer);
end entity;

architecture a of t_vhdl_datagennest_leaf is
begin
  o <= d'length;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_datagennest_pkg.all;

entity t_vhdl_datagennest_mid is
  generic (m : mid_t);
  port (d : in std_logic_vector(m.leaf.len - 1 downto 0); o : out integer);
end entity;

architecture a of t_vhdl_datagennest_mid is
begin
  -- Only where the leaf is wide enough to be worth one
  g : if m.leaf.len > 4 generate
    u : entity work.t_vhdl_datagennest_leaf
      generic map (h => m.leaf)
      port map (d => d, o => o);
  else generate
    o <= -1;
  end generate;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_datagennest_pkg.all;

-- Two levels between the constant and the leaf
entity t_vhdl_datagennest_outer is
  generic (t : top_t);
  port (d : in std_logic_vector(t.mid.leaf.len + t.extra - 1 downto 0); o : out integer);
end entity;

architecture a of t_vhdl_datagennest_outer is
begin
  u : entity work.t_vhdl_datagennest_mid
    generic map (m => t.mid)
    port map (d => d(t.mid.leaf.len - 1 downto 0), o => o);
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_datagennest_pkg.all;

entity t_vhdl_datagennest is
end entity;

architecture a of t_vhdl_datagennest is
  constant l : leaf_t := new_leaf(4);
  constant m8 : mid_t := new_mid(8);
  constant m3 : mid_t := new_mid(3);
  constant t12 : top_t := new_top(12, 2);
  signal d4 : std_logic_vector(3 downto 0) := (others => '0');
  signal d8 : std_logic_vector(7 downto 0) := (others => '0');
  signal d3 : std_logic_vector(2 downto 0) := (others => '0');
  signal d14 : std_logic_vector(13 downto 0) := (others => '0');
  signal o1, o2, o3, o4 : integer;
begin
  u1 : entity work.t_vhdl_datagennest_leaf generic map (h => l) port map (d => d4, o => o1);
  u2 : entity work.t_vhdl_datagennest_mid generic map (m => m8) port map (d => d8, o => o2);
  u3 : entity work.t_vhdl_datagennest_mid generic map (m => m3) port map (d => d3, o => o3);
  u4 : entity work.t_vhdl_datagennest_outer generic map (t => t12) port map (d => d14, o => o4);

  process
  begin
    wait for 1 ns;
    assert o1 = 4 severity failure;
    assert o2 = 8 severity failure;
    assert o3 = -1 severity failure;
    assert o4 = 12 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
