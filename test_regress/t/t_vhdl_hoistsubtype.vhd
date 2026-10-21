-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A generic record built by an impure function travels as data, since one
-- element (an id a protected counter hands out) folds nowhere; the widths
-- beside it are what the architecture's subtypes are bounded by, and a
-- signal is constrained by those subtypes: "subtype id_range_c is natural
-- range instance.id_width - 1 downto 0", then "signal s : ms_t(id(id_range_c),
-- ...)". Each instance works its subtypes out (open-logic's AXI master VC).

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_hoistsubtype_pkg is
  type counter_t is protected
    impure function next_id return natural;
  end protected;
  type handle_t is record
    p_id : natural;
    id_width : natural;
    data_width : natural;
  end record;
  impure function new_handle(data_width : natural; id_width : natural := 0) return handle_t;
  type ms_t is record
    id : std_logic_vector;
    data : std_logic_vector;
  end record;
end package;
package body t_vhdl_hoistsubtype_pkg is
  type counter_t is protected body
    variable n : natural := 0;
    impure function next_id return natural is
    begin
      n := n + 1;
      return n;
    end function;
  end protected body;
  shared variable counter : counter_t;
  impure function new_handle(data_width : natural; id_width : natural := 0) return handle_t is
  begin
    return (p_id => counter.next_id, id_width => id_width, data_width => data_width);
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_hoistsubtype_pkg.all;
entity t_vhdl_hoistsubtype_vc is
  generic (instance : handle_t);
  port (o : out ms_t);
end entity;
architecture a of t_vhdl_hoistsubtype_vc is
  subtype id_range_c is natural range instance.id_width - 1 downto 0;
  subtype data_range_c is natural range instance.data_width - 1 downto 0;
  signal ms_i : ms_t(id(id_range_c), data(data_range_c));
begin
  o <= ms_i;
  process
  begin
    ms_i.id <= (others => '1');
    ms_i.data <= (others => '0');
    ms_i.data(0) <= '1';
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_hoistsubtype_pkg.all;
entity t_vhdl_hoistsubtype is
end entity;
architecture a of t_vhdl_hoistsubtype is
  constant h1_c : handle_t := new_handle(data_width => 8, id_width => 3);
  constant h2_c : handle_t := new_handle(data_width => 12, id_width => 1);
  signal o1 : ms_t(id(2 downto 0), data(7 downto 0));
  signal o2 : ms_t(id(0 downto 0), data(11 downto 0));
begin
  u1 : entity work.t_vhdl_hoistsubtype_vc generic map (instance => h1_c) port map (o => o1);
  u2 : entity work.t_vhdl_hoistsubtype_vc generic map (instance => h2_c) port map (o => o2);
  process
  begin
    wait for 2 ns;
    assert o1.id = "111" and o1.data = x"01" report "o1 " & to_string(o1.id) & " "
      & to_string(o1.data) severity failure;
    assert o2.id = "1" and o2.data = x"001" report "o2 " & to_string(o2.id) & " "
      & to_string(o2.data) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
