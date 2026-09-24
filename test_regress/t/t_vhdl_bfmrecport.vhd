-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A verification component hands its interface port -- an unconstrained
-- record, as long as the signal an instance connects -- to a BFM, together
-- with an array of words whose element the BFM's formal leaves open.  The
-- port's elements take their bounds from generics the instance binds, so
-- they are lengths, and the call gets the copy the array of words needs.

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_bfmrecport_pkg is
   type t_slv_array is array (natural range <>) of std_logic_vector;
   type t_if is record
      tdata  : std_logic_vector;
      tvalid : std_logic;
   end record;
   procedure send_words (constant words : in t_slv_array; signal ifc : inout t_if);
end package;

package body t_vhdl_bfmrecport_pkg is
   procedure send_words (constant words : in t_slv_array; signal ifc : inout t_if) is
   begin
      for i in words'range loop
         ifc.tdata  <= words(i);
         ifc.tvalid <= '1';
         wait for 1 ns;
      end loop;
      ifc.tvalid <= '0';
   end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_bfmrecport_pkg.all;

entity t_vhdl_bfmrecport_vvc is
   generic (GW : natural := 8);
   port (vif : inout t_if);
end entity;

architecture a of t_vhdl_bfmrecport_vvc is
begin
   p : process
      variable w : t_slv_array(0 to 2)(GW - 1 downto 0);
   begin
      w(0) := (others => '0');
      w(1) := (others => '1');
      w(2) := (others => '0');
      w(2)(0) := '1';
      send_words(w, vif);
      wait;
   end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_bfmrecport_pkg.all;

entity t_vhdl_bfmrecport is
end entity;

architecture a of t_vhdl_bfmrecport is
   signal s8 : t_if(tdata(7 downto 0));
   signal s12 : t_if(tdata(11 downto 0));
begin
   u8 : entity work.t_vhdl_bfmrecport_vvc generic map (GW => 8) port map (vif => s8);
   u12 : entity work.t_vhdl_bfmrecport_vvc generic map (GW => 12) port map (vif => s12);

   chk : process
   begin
      wait for 0.5 ns;
      assert s8.tdata = x"00" and s12.tdata = x"000" report "word 0" severity failure;
      wait for 1 ns;
      assert s8.tdata = x"ff" and s12.tdata = x"fff" report "word 1" severity failure;
      wait for 1 ns;
      assert s8.tdata = x"01" and s12.tdata = x"001" report "word 2" severity failure;
      wait for 1 ns;
      assert s8.tvalid = '0' and s12.tvalid = '0' report "done" severity failure;
      report "*-* All Finished *-*";
      wait;
   end process;
end architecture;
