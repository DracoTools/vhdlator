-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A verification component's interface port is an unconstrained record, as
-- long as the signal an instance connects, and a procedure in a package
-- drives it through a signal parameter.  The formal is written with the open
-- subtype, but it names the port, whose elements take their bounds from the
-- instance; a value carrying its own bounds written into one of them has to
-- be converted to that length rather than emitted as it stands.

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_bfmdrive_pkg is
   type t_if is record
      tdata  : std_logic_vector;
      tvalid : std_logic;
   end record;
   procedure send (constant d : in std_logic_vector; signal ifc : inout t_if);
end package;

package body t_vhdl_bfmdrive_pkg is
   procedure send (constant d : in std_logic_vector; signal ifc : inout t_if) is
   begin
      ifc.tdata  <= d;
      ifc.tvalid <= '1';
      wait for 1 ns;
      ifc.tvalid <= '0';
   end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_bfmdrive_pkg.all;

entity t_vhdl_bfmdrive_vvc is
   generic (GW : natural := 8);
   port (vif : inout t_if);
end entity;

architecture a of t_vhdl_bfmdrive_vvc is
begin
   p : process
      variable v : std_logic_vector(GW - 1 downto 0) := (others => '0');
   begin
      v(0) := '1';
      v(GW - 1) := '1';
      send(v, vif);
      wait;
   end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_bfmdrive_pkg.all;

entity t_vhdl_bfmdrive is
end entity;

architecture a of t_vhdl_bfmdrive is
   signal s8 : t_if(tdata(7 downto 0));
   signal s12 : t_if(tdata(11 downto 0));
begin
   -- Two instances, two lengths for the one port
   u8 : entity work.t_vhdl_bfmdrive_vvc generic map (GW => 8) port map (vif => s8);
   u12 : entity work.t_vhdl_bfmdrive_vvc generic map (GW => 12) port map (vif => s12);

   chk : process
   begin
      wait until s8.tvalid = '1' and s12.tvalid = '1';
      assert s8.tdata = x"81" report "8-bit port" severity failure;
      assert s12.tdata = x"801" report "12-bit port" severity failure;
      report "*-* All Finished *-*";
      wait;
   end process;
end architecture;
