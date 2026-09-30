-- DESCRIPTION: Verilator: VHDL record aggregate sized by elaboration
--
-- This file is part of Verilator.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
--
-- A record variable whose elements are constrained by a signal a generic
-- sizes, given an aggregate of another width in a branch that does not run
-- for the width the generic is given. The element's length is not locally
-- static, so the mismatch is a condition to be met while running
-- (10.6.2.1), not an error of the source: VUnit's tb_axi_stream assigns
-- "tdata => x"99"" in a test case its run.py never runs at 16 bits.

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_elabrecagg_pkg is
  type handle_t is record
    id  : natural;
    len : natural;
  end record;
  type trans_t is record
    data : std_logic_vector;
    last : boolean;
  end record;
  type counter_t is protected
    impure function bump return natural;
  end protected;
  impure function new_handle(len : natural) return handle_t;
  impure function data_length(h : handle_t) return natural;
end package;

package body t_vhdl_elabrecagg_pkg is
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

  impure function data_length(h : handle_t) return natural is
  begin
    return h.len;
  end;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_elabrecagg_pkg.all;

entity t_vhdl_elabrecagg is
  generic (g_len : positive := 8);
end entity;

architecture a of t_vhdl_elabrecagg is
  constant h : handle_t := new_handle(g_len);
  signal tdata : std_logic_vector(data_length(h) - 1 downto 0) := (others => '0');
begin
  process
    variable t : trans_t(data(tdata'range));
  begin
    if g_len = 8 then
      t := (data => x"99", last => true);
    else
      t := (data => x"1234", last => false);
    end if;
    report "len " & integer'image(t.data'length) & " last " & boolean'image(t.last);
    assert t.data'length = g_len report "length" severity failure;
    assert t.last = (g_len = 8) report "branch" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
