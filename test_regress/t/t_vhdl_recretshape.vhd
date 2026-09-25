-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A function that builds a record whose elements have no length of their
-- own in the type, constrained by its parameters, and gives it back: the
-- widths of UVVM's init_*_if_signals functions (bitvis_vip_avalon_st/src/
-- avalon_st_bfm_pkg.vhd). The copy made for a call held some elements as
-- bits and gave the record back in the shape that carries every one of
-- them; neither "return v", a signal's initial value, a signal assignment
-- nor a variable assignment converted between the two shapes. The first
-- three stopped the C++ compiler; the last was refused.
library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_recretshape_pkg is
  type if_t is record
    data : std_logic_vector;
    ch : std_logic_vector;
    valid : std_logic;
  end record;
  function init_if(is_master : boolean; dw, cw : natural) return if_t;
end package;

package body t_vhdl_recretshape_pkg is
  function init_if(is_master : boolean; dw, cw : natural) return if_t is
    variable v : if_t(data(dw - 1 downto 0), ch(cw - 1 downto 0));
  begin
    if is_master then
      v.data := (v.data'range => '0');
      v.ch := (v.ch'range => '0');
      v.valid := '0';
    else
      v.data := (v.data'range => '1');
      v.ch := (v.ch'range => '1');
      v.valid := '1';
    end if;
    return v;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_recretshape_pkg.all;

entity t_vhdl_recretshape is
  generic (DW : natural := 8);
end entity;

architecture a of t_vhdl_recretshape is
  signal s : if_t(data(DW - 1 downto 0), ch(1 downto 0)) := init_if(true, DW, 2);
begin
  process
    variable x : if_t(data(DW - 1 downto 0), ch(1 downto 0));
  begin
    assert s.data = x"00" and s.ch = "00" and s.valid = '0' severity failure;
    x := init_if(false, DW, 2);
    assert x.data = x"ff" and x.ch = "11" and x.valid = '1' severity failure;
    s <= init_if(false, DW, 2);
    wait for 1 ns;
    assert s.data = x"ff" and s.ch = "11" and s.valid = '1' severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
