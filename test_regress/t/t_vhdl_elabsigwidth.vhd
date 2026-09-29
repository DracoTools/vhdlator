-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- Signals of the top-level architecture sized through an accessor of a
-- handle record an impure function builds -- VUnit's "signal data :
-- std_logic_vector(data_length(avalon_source_stream) - 1 downto 0)". A
-- signal is declared where elaboration declares it once (6.4.2.3), so its
-- bounds are what elaboration makes of them, though part of the handle is
-- an id only running hands out.

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_elabsigwidth_pkg is
  type counter_t is protected
    impure function next_id return natural;
  end protected;

  type tag_t is record
    p_id : natural;
  end record;
  constant null_tag : tag_t := (p_id => 0);

  type handle_t is record
    p_id   : natural;
    p_tag  : tag_t;
    p_len  : positive;
    p_alen : positive;
  end record;

  impure function new_tag return tag_t;
  constant default_tag : tag_t;
  impure function new_handle(len : positive; alen : positive := 4;
                             tag : tag_t := default_tag;
                             id : natural := 0) return handle_t;
  impure function len_of(h : handle_t) return positive;
  impure function alen_of(h : handle_t) return positive;
end package;

package body t_vhdl_elabsigwidth_pkg is
  type counter_t is protected body
    variable n : natural := 0;
    impure function next_id return natural is
    begin
      n := n + 1;
      return n;
    end function;
  end protected body;

  shared variable counter : counter_t;

  impure function new_tag return tag_t is
  begin
    return (p_id => counter.next_id);
  end function;

  constant default_tag : tag_t := new_tag;

  impure function new_handle(len : positive; alen : positive := 4;
                             tag : tag_t := default_tag;
                             id : natural := 0) return handle_t is
    variable v_id : natural;
  begin
    v_id := id when id /= 0 else counter.next_id;
    return (p_id => v_id, p_tag => tag, p_len => len, p_alen => alen);
  end function;

  impure function len_of(h : handle_t) return positive is
  begin
    return h.p_len;
  end function;

  impure function alen_of(h : handle_t) return positive is
  begin
    return h.p_alen;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_elabsigwidth_pkg.all;

entity t_vhdl_elabsigwidth_sub is
  generic (h : handle_t);
  port (data : out std_logic_vector(len_of(h) - 1 downto 0);
        addr : out std_logic_vector(alen_of(h) - 1 downto 0));
end entity;

architecture a of t_vhdl_elabsigwidth_sub is
begin
  data <= (others => '1');
  addr <= (others => '0');
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_elabsigwidth_pkg.all;

entity t_vhdl_elabsigwidth is
end entity;

architecture a of t_vhdl_elabsigwidth is
  constant h1 : handle_t := new_handle(len => 12);
  constant h2 : handle_t := new_handle(len => 6, alen => 3, id => 9);
  signal d1 : std_logic_vector(len_of(h1) - 1 downto 0);
  signal a1 : std_logic_vector(alen_of(h1) - 1 downto 0);
  signal d2 : std_logic_vector(len_of(h2) - 1 downto 0);
  signal a2 : std_logic_vector(alen_of(h2) - 1 downto 0);
begin
  i1 : entity work.t_vhdl_elabsigwidth_sub generic map (h => h1) port map (data => d1, addr => a1);
  i2 : entity work.t_vhdl_elabsigwidth_sub generic map (h => h2) port map (data => d2, addr => a2);

  process
    -- A process is elaborated once as well (14.4.2)
    variable pv : std_logic_vector(len_of(h2) - 1 downto 0);
  begin
    wait for 1 ns;
    assert pv'length = 6 report "pv length " & integer'image(pv'length) severity failure;
    assert d1'length = 12 and a1'length = 4 and d2'length = 6 and a2'length = 3
      report "lengths " & integer'image(d1'length) & " " & integer'image(a1'length) & " "
             & integer'image(d2'length) & " " & integer'image(a2'length) severity failure;
    assert d1 = x"fff" and d2 = "111111" and a1 = "0000" and a2 = "000"
      report "values" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
