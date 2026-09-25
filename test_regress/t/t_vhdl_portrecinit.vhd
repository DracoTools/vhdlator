-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A record type used on a port, so kept as the bits elaboration gives it,
-- and a function that builds one of it constrained by its parameters and
-- gives it back: UVVM's init_axistream_if_signals (bitvis_vip_axistream/
-- src/axistream_bfm_pkg.vhd), called from a procedure on its signal
-- formal with the widths of the signal the call passed. A port record never
-- carried its elements, so the function's result had no type that could be
-- emitted; and the record it gave back was put into the signal without
-- being converted from the carried shape to the signal's bits.
library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_portrecinit_pkg is
  type t_if is record
    tdata : std_logic_vector;
    tuser : std_logic_vector;
    tvalid : std_logic;
  end record;
  function init_if(is_master : boolean; data_width, user_width : natural) return t_if;
  procedure reset_if(signal ifc : inout t_if; constant is_master : boolean);
end package;

package body t_vhdl_portrecinit_pkg is
  function init_if(is_master : boolean; data_width, user_width : natural) return t_if is
    variable v : t_if(tdata(data_width - 1 downto 0), tuser(user_width - 1 downto 0));
  begin
    if is_master then
      v.tdata := (v.tdata'range => '0');
      v.tuser := (v.tuser'range => '0');
      v.tvalid := '0';
    else
      v.tdata := (v.tdata'range => '1');
      v.tuser := (v.tuser'range => '1');
      v.tvalid := '1';
    end if;
    return v;
  end function;

  procedure reset_if(signal ifc : inout t_if; constant is_master : boolean) is
  begin
    ifc <= init_if(is_master, ifc.tdata'length, ifc.tuser'length);
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_portrecinit_pkg.all;

entity t_vhdl_portrecinit_bfm is
  generic (GW : natural);
  port (ifc : inout t_if);
end entity;

architecture a of t_vhdl_portrecinit_bfm is
begin
  process
  begin
    reset_if(ifc, false);
    wait for 1 ns;
    assert ifc.tdata = (GW - 1 downto 0 => '1') and ifc.tuser = "11" and ifc.tvalid = '1'
      severity failure;
    reset_if(ifc, true);
    wait for 1 ns;
    assert ifc.tdata = (GW - 1 downto 0 => '0') and ifc.tuser = "00" and ifc.tvalid = '0'
      severity failure;
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_portrecinit_pkg.all;

entity t_vhdl_portrecinit is
  generic (GC_DATA_WIDTH : natural := 12);
end entity;

architecture a of t_vhdl_portrecinit is
  signal ifc : t_if(tdata(GC_DATA_WIDTH - 1 downto 0), tuser(1 downto 0));
begin
  bfm : entity work.t_vhdl_portrecinit_bfm
    generic map (GW => GC_DATA_WIDTH)
    port map (ifc => ifc);

  process
  begin
    wait for 1 ns;
    assert ifc.tdata = x"fff" and ifc.tvalid = '1' severity failure;
    wait for 1 ns;
    assert ifc.tdata = x"000" and ifc.tvalid = '0' severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
