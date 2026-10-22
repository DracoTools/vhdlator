-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A package's constant of a record of logic elements, read by a function of
-- the package that an architecture's constant calls: the constant's value is
-- worked out as the design is elaborated, and so is what the function reads.
-- Shaped after surf's AxiStreamPkg, "ret := AXI_STREAM_MASTER_INIT_C" in
-- axiStreamMasterInit, called for AxiStreamSplitter's REG_INIT_C.

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_pkgrecconst_pkg is
  type MType is record
    tValid : std_logic;
    tKeep : std_logic_vector(7 downto 0);
    tData : std_logic_vector(63 downto 0);
  end record;
  constant M_INIT_C : MType := (tValid => '0', tKeep => (others => '1'), tData => (others => '0'));
  function mInit (n : natural) return MType;
end package;

package body t_vhdl_pkgrecconst_pkg is
  function mInit (n : natural) return MType is
    variable ret : MType;
  begin
    ret := M_INIT_C;
    if n > 2 then
      ret.tKeep := x"0F";
    end if;
    return ret;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_pkgrecconst_pkg.all;

entity t_vhdl_pkgrecconst is
  generic (N_G : positive := 4);
end entity;

architecture rtl of t_vhdl_pkgrecconst is
  type MArray is array (natural range <>) of MType;
  type RegType is record
    masters : MArray(1 downto 0);
    count : natural;
  end record;
  constant REG_INIT_C : RegType := (masters => (others => mInit(N_G)), count => 0);
  constant KEEP_C : std_logic_vector(7 downto 0) := REG_INIT_C.masters(1).tKeep;
  signal r : RegType := REG_INIT_C;
  signal w : std_logic_vector(KEEP_C'range) := KEEP_C;
begin
  process
  begin
    wait for 1 ns;
    assert r.masters(1).tKeep = x"0F" and r.masters(0).tValid = '0' report "r" severity failure;
    assert w = x"0F" report "w" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
