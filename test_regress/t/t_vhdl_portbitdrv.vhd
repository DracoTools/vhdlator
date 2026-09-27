-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A procedure that drives a record port of mode inout, whole and then a bit
-- of one element at a time: UVVM's axistream_transmit clears the interface
-- with "axistream_if <= init_axistream_if_signals(...)" and then sets
-- "axistream_if.tkeep(v_byte_in_word) <= '1'" per byte
-- (bitvis_vip_axistream/src/axistream_bfm_pkg.vhd). Inside a subprogram
-- such a port is driven through a driver of the call's own; the whole
-- assignments went there, but a bit of a member did not, and wrote the port
-- directly. The driver's own copy never had the bit, so clearing it changed
-- nothing, and a packet of one byte after one of four still had all four
-- bytes' keep bits set.
library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_portbitdrv_pkg is
  type t_if is record
    tdata : std_logic_vector;
    tkeep : std_logic_vector;
    tvalid : std_logic;
    tready : std_logic;
  end record;
  function init_if(is_master : boolean; w : natural) return t_if;
  procedure transmit(constant n : natural; signal clk : in std_logic; signal ifc : inout t_if);
end package;
package body t_vhdl_portbitdrv_pkg is
  function init_if(is_master : boolean; w : natural) return t_if is
    variable v : t_if(tdata(w - 1 downto 0), tkeep(w / 8 - 1 downto 0));
  begin
    if is_master then
      v.tdata := (others => '0');
      v.tkeep := (others => '0');
      v.tvalid := '0';
      v.tready := 'Z';
    else
      v.tdata := (others => 'Z');
      v.tkeep := (others => 'Z');
      v.tvalid := 'Z';
      v.tready := '0';
    end if;
    return v;
  end function;
  procedure transmit(constant n : natural; signal clk : in std_logic; signal ifc : inout t_if) is
  begin
    ifc <= init_if(true, ifc.tdata'length);
    wait until rising_edge(clk);
    for i in 0 to n - 1 loop
      ifc.tkeep(i) <= '1';
    end loop;
    ifc.tvalid <= '1';
    wait until rising_edge(clk);
    assert ifc.tkeep = (ifc.tkeep'range => '0') or n > 0 severity failure;
    for i in ifc.tkeep'range loop
      assert (ifc.tkeep(i) = '1') = (i < n)
        report "sent " & integer'image(n) & " tkeep=" & to_string(ifc.tkeep) severity failure;
    end loop;
    ifc <= init_if(true, ifc.tdata'length);
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_portbitdrv_pkg.all;
entity t_vhdl_portbitdrv_vvc is
  port (clk : in std_logic; ifc : inout t_if := init_if(true, 32));
end entity;
architecture a of t_vhdl_portbitdrv_vvc is
begin
  process
  begin
    transmit(4, clk, ifc);
    transmit(1, clk, ifc);
    transmit(3, clk, ifc);
    transmit(2, clk, ifc);
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_portbitdrv_pkg.all;
entity t_vhdl_portbitdrv_sink is
  port (clk : in std_logic; ifc : inout t_if := init_if(false, 32));
end entity;
architecture a of t_vhdl_portbitdrv_sink is
begin
  process
  begin
    ifc <= init_if(false, 32);
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_portbitdrv_pkg.all;
entity t_vhdl_portbitdrv is
end entity;
architecture a of t_vhdl_portbitdrv is
  signal clk : std_logic := '0';
  signal ifc : t_if(tdata(31 downto 0), tkeep(3 downto 0));
begin
  clk <= not clk after 5 ns;
  m : entity work.t_vhdl_portbitdrv_vvc port map (clk => clk, ifc => ifc);
  s : entity work.t_vhdl_portbitdrv_sink port map (clk => clk, ifc => ifc);
  process
  begin
    wait for 100 ns;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
