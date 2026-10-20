-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- From OSVVM 2026.09, which its own regression found (wave 4):
-- * an aggregate target is of a composite type (9.3.3.1), so of an overloaded
--   call returning INTEGER or INTEGER_VECTOR only the vector can be assigned
--   to it: ClockResetPkg's "(burst, variance) := RandCovPoint(id)";
-- * "alias rd : def.ReadData'subtype is def.ReadData" names an element of the
--   object def, though a procedure ReadData is visible (8.3);
-- * a generic's default "-" & X"--_----" has no type of its own, and the
--   bounds of an open generic taken from it are asked of it as the generic's
--   type (WishboneRegisterSubordinate);
-- * "(others => '0')" assigned to an element of a port whose record subtype
--   constrains that element (InterruptGeneratorBit).

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_osvvmidioms_pkg is
  type rd_t is record
    data : std_logic_vector(7 downto 0);
  end record;
  type bus_t is record
    ReadData : rd_t;
    WriteData : rd_t;
  end record;
  procedure ReadData(variable d : out integer);
  function pick(n : integer) return integer;
  function pick(n : integer) return integer_vector;
  type rec_t is record
    tomodel : std_ulogic_vector;
    frommodel : std_ulogic_vector;
  end record;
  subtype rec1_t is rec_t(tomodel(0 downto 0), frommodel(1 downto 0));
end package;

package body t_vhdl_osvvmidioms_pkg is
  procedure ReadData(variable d : out integer) is
  begin
    d := 1;
  end procedure;
  function pick(n : integer) return integer is
  begin
    return n;
  end function;
  function pick(n : integer) return integer_vector is
  begin
    return (n, n + 1);
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_osvvmidioms_pkg.all;

entity t_vhdl_osvvmidioms_sub is
  port (tr : inout rec1_t);
end entity;

architecture a of t_vhdl_osvvmidioms_sub is
begin
  process
  begin
    tr.frommodel <= (others => '1');
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_osvvmidioms_pkg.all;

entity t_vhdl_osvvmidioms is
  generic (
    ADDR : std_logic_vector := "-" & X"--_----";
    REG : std_logic_vector := "0" & X"00_----"
  );
end entity;

architecture a of t_vhdl_osvvmidioms is
  signal tr : rec1_t;
begin
  u : entity work.t_vhdl_osvvmidioms_sub port map (tr => tr);
  process
    variable def : bus_t;
    alias LWD : def.WriteData'subtype is def.WriteData;
    alias LRD : def.ReadData'subtype is def.ReadData;
    variable burst, variance : integer;
  begin
    (burst, variance) := pick(4);
    assert burst = 4 and variance = 5 report "aggregate target" severity failure;
    LRD.data := x"5a";
    assert def.ReadData.data = x"5a" report "alias of an element" severity failure;
    assert ADDR'length = 25 and REG'length = 25 report "generic default" severity failure;
    assert REG(REG'left) = '0' report "generic default value" severity failure;
    wait for 1 ns;
    assert tr.frommodel = "11" report "port record element" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
