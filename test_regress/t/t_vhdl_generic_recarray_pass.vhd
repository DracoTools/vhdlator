-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A generic that is an array, indexed by an enumeration, of arrays of records
-- with elements of open length, handed on from one entity to another: UVVM's
-- Ethernet VVC giving its GC_DUT_IF_FIELD_CONFIG to its transmitting VVC. The
-- bounds of the elements and of the records' elements are the actual's own,
-- not the outer array's, and the value is the package's constant written as
-- the record each entity declares for itself.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package t_vhdl_generic_recarray_pass_pkg is
  type t_direction is (TRANSMIT, RECEIVE);
  type t_cfg is record
    addr  : unsigned;
    width : positive;
    descr : string;
  end record;
  type t_cfg_array is array (natural range <>) of t_cfg;
  type t_cfg_dir_array is array (t_direction range <>) of t_cfg_array;
  constant C_CFG : t_cfg(addr(3 downto 0)) := (addr => "0101", width => 8, descr => "default");
  constant C_DEFAULT : t_cfg_dir_array(t_direction'low to t_direction'high)(0 to 0)(addr(3 downto 0), descr(1 to 7))
    := (others => (others => C_CFG));
  constant C_TWO : t_cfg_dir_array(TRANSMIT to RECEIVE)(0 to 1)(addr(1 downto 0), descr(1 to 3))
    := (TRANSMIT => (0 => ("01", 16, "tx0"), 1 => ("10", 24, "tx1")),
        RECEIVE  => (0 => ("11", 32, "rx0"), 1 => ("00", 40, "rx1")));
end package;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.t_vhdl_generic_recarray_pass_pkg.all;

entity t_vhdl_generic_recarray_pass_sub is
  generic (G : t_cfg_dir_array := C_DEFAULT; WANT : natural := 8; LEN : natural := 1);
end entity;

architecture a of t_vhdl_generic_recarray_pass_sub is
begin
  process
  begin
    assert G'length = 2 and G(RECEIVE)'length = LEN severity failure;
    assert G(RECEIVE)(G(RECEIVE)'high).width = WANT severity failure;
    report integer'image(G(TRANSMIT)(0).width) & " " & G(TRANSMIT)(0).descr & " "
      & to_hstring(G(RECEIVE)(0).addr);
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_generic_recarray_pass_pkg.all;

entity t_vhdl_generic_recarray_pass is
  generic (G : t_cfg_dir_array := C_TWO);
end entity;

architecture a of t_vhdl_generic_recarray_pass is
begin
  -- Its own default
  d : entity work.t_vhdl_generic_recarray_pass_sub;
  -- The constant itself
  c : entity work.t_vhdl_generic_recarray_pass_sub generic map (G => C_TWO, WANT => 40, LEN => 2);
  -- The generic of the entity above, handed on
  h : entity work.t_vhdl_generic_recarray_pass_sub generic map (G => G, WANT => 40, LEN => 2);
  process
  begin
    wait for 1 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
