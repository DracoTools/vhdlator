-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A generic record built by an impure function travels as data; a variable
-- is sized by one of its elements, and a slice in a statement is written
-- with the same element: "sr := '0' & sr(instance.width - 1 downto 1)".
-- The slice's width is read off the bound the declaration hoisted, so it is
-- the instance's number. Shaped after open-logic's SPI slave verification
-- component, "shift_reg_tx(instance.max_trans_width - 1 downto 1)".

package t_vhdl_hoistslice_pkg is
  type counter_t is protected
    impure function next_id return natural;
  end protected;
  type handle_t is record
    p_id : natural;
    width : natural;
  end record;
  impure function new_handle(width : natural) return handle_t;
end package;
package body t_vhdl_hoistslice_pkg is
  type counter_t is protected body
    variable n : natural := 0;
    impure function next_id return natural is
    begin
      n := n + 1;
      return n;
    end function;
  end protected body;
  shared variable counter : counter_t;
  impure function new_handle(width : natural) return handle_t is
  begin
    return (p_id => counter.next_id, width => width);
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_hoistslice_pkg.all;
entity t_vhdl_hoistslice_vc is
  generic (instance : handle_t);
  port (o : out std_logic_vector(7 downto 0));
end entity;
architecture a of t_vhdl_hoistslice_vc is
begin
  process
    variable sr : std_logic_vector(instance.width - 1 downto 0) := (others => '1');
  begin
    sr := '0' & sr(instance.width - 1 downto 1);
    sr := sr(instance.width - 2 downto 0) & '0';
    o <= (others => '0');
    o(instance.width - 1 downto 0) <= sr;
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_hoistslice_pkg.all;
entity t_vhdl_hoistslice is
end entity;
architecture a of t_vhdl_hoistslice is
  constant h1_c : handle_t := new_handle(4);
  constant h2_c : handle_t := new_handle(8);
  signal o1, o2 : std_logic_vector(7 downto 0);
begin
  u1 : entity work.t_vhdl_hoistslice_vc generic map (instance => h1_c) port map (o => o1);
  u2 : entity work.t_vhdl_hoistslice_vc generic map (instance => h2_c) port map (o => o2);
  process
  begin
    wait for 1 ns;
    assert o1 = "00001110" report "o1 " & to_string(o1) severity failure;
    assert o2 = "11111110" report "o2 " & to_string(o2) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
