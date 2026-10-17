-- DESCRIPTION: Verilator: VHDL inout record-of-records port given its default by a function of a generic
--
-- The port's elements are records with open elements, so the default is a
-- value of a shape only an instance settles: worked out per instance over
-- the generic it is handed, and the port's driver starts at it. An element
-- nothing inside drives reads the default, not 'U' (bugs.md 388mb, from
-- UVVM's AXI VVCs).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_portdflt_inst_pkg is
  type ch_t is record
    id : std_logic_vector;
    valid : std_logic;
  end record;
  type if_t is record
    wa : ch_t;
    rd : ch_t;
  end record;
  function init_if(w : natural) return if_t;
end package;

package body t_vhdl_portdflt_inst_pkg is
  function init_if(w : natural) return if_t is
    variable v : if_t(wa(id(w - 1 downto 0)), rd(id(w - 1 downto 0)));
  begin
    v.wa.id := (others => '0');
    v.wa.valid := '0';
    v.rd.id := (others => 'Z');
    v.rd.valid := 'Z';
    return v;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_portdflt_inst_pkg.all;

entity t_vhdl_portdflt_inst_vvc is
  generic (W : natural := 2);
  port (m : inout if_t := init_if(W));
end entity;

architecture a of t_vhdl_portdflt_inst_vvc is
begin
  process
  begin
    m.wa.valid <= '1';
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_portdflt_inst_pkg.all;

entity t_vhdl_portdflt_inst is
end entity;

architecture a of t_vhdl_portdflt_inst is
  signal s : if_t(wa(id(3 downto 0)), rd(id(3 downto 0)));
begin
  u : entity work.t_vhdl_portdflt_inst_vvc generic map (W => 4) port map (m => s);

  process
  begin
    wait for 1 ns;
    assert s.wa.id = "0000" report "wa.id " & to_string(s.wa.id) severity failure;
    assert s.wa.valid = '1' severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
