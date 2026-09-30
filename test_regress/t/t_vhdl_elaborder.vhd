-- DESCRIPTION: Verilator: VHDL handle relayed through an entity read first
--
-- This file is part of Verilator.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
--
-- An entity hands its own record generic to an instance whose port widths
-- and generate conditions are read out of it, and is written before the
-- entity it instantiates -- the way VUnit's axi_stream_master comes before
-- its monitor. The generic map's formals name generics of an entity not yet
-- read, and are bound by name when the design is elaborated.

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_elaborder_pkg is
  type handle_t is record
    id  : natural;
    len : natural;
  end record;
  constant null_handle : handle_t := (id => 0, len => 0);
  type counter_t is protected
    impure function bump return natural;
  end protected;
  impure function new_handle(len : natural) return handle_t;
end package;

package body t_vhdl_elaborder_pkg is
  type counter_t is protected body
    variable count : natural := 0;
    impure function bump return natural is
    begin
      count := count + 1;
      return count;
    end;
  end protected body;

  shared variable counter : counter_t;

  impure function new_handle(len : natural) return handle_t is
  begin
    return (id => counter.bump, len => len);
  end;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_elaborder_pkg.all;

entity t_vhdl_elaborder_mid is
  generic (w : handle_t);
  port (d : in std_logic_vector(w.len - 1 downto 0));
end entity;

architecture a of t_vhdl_elaborder_mid is
begin
  g : if w /= null_handle generate
    c : entity work.t_vhdl_elaborder_leaf
      generic map (h => w)
      port map (d => d);
  end generate;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_elaborder_pkg.all;

entity t_vhdl_elaborder_leaf is
  generic (h : handle_t);
  port (d : in std_logic_vector(h.len - 1 downto 0));
end entity;

architecture a of t_vhdl_elaborder_leaf is
begin
  process
  begin
    wait for 1 ns;
    assert d'length = 6 report "leaf width " & integer'image(d'length) severity failure;
    assert d = "101100" report "leaf data" severity failure;
    report "leaf checked";
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_elaborder_pkg.all;

entity t_vhdl_elaborder is
end entity;

architecture a of t_vhdl_elaborder is
  constant h : handle_t := new_handle(len => 6);
  signal d : std_logic_vector(5 downto 0) := (others => '0');
begin
  m : entity work.t_vhdl_elaborder_mid
    generic map (w => h)
    port map (d => d);

  process
  begin
    d <= "101100";
    wait for 2 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
