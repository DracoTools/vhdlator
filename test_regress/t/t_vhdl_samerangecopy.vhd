-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A function copies a slice of one variable into the same slice of another,
-- "ret(top downto 0) := tmp(top downto 0)", with top a variable, and is
-- called for a constant handed to an instance's generic, so it is worked out
-- as the design is elaborated. std_logic_unsigned's "+" joins it with a
-- generic. Shaped after surf's StdRtlPkg resize, called by AdcDdrCore.

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;

package t_vhdl_samerangecopy_pkg is
  function resize (vec : std_logic_vector; newSize : integer; pad : std_logic := '0') return std_logic_vector;
  function minimum (left, right : integer) return integer;
end package;

package body t_vhdl_samerangecopy_pkg is
  function minimum (left, right : integer) return integer is
  begin
    if left < right then
      return left;
    end if;
    return right;
  end function;
  function resize (vec : std_logic_vector; newSize : integer; pad : std_logic := '0') return std_logic_vector is
    variable ret : std_logic_vector(newSize - 1 downto 0);
    variable tmp : std_logic_vector(vec'length - 1 downto 0);
    variable top : integer;
  begin
    ret := (others => pad);
    tmp := vec;
    top := minimum(newSize, vec'length) - 1;
    ret(top downto 0) := tmp(top downto 0);
    return ret;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_samerangecopy_child is
  generic (A_G : std_logic_vector(31 downto 0));
  port (o : out std_logic_vector(31 downto 0));
end entity;
architecture rtl of t_vhdl_samerangecopy_child is
begin
  o <= A_G;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;
use work.t_vhdl_samerangecopy_pkg.all;

entity t_vhdl_samerangecopy is
  generic (BASE_G : std_logic_vector(31 downto 0) := x"80000000");
end entity;

architecture rtl of t_vhdl_samerangecopy is
  constant K_C : std_logic_vector(11 downto 0) := x"800";
  constant A_C : std_logic_vector(31 downto 0) := BASE_G + resize(K_C, 32);
  signal o : std_logic_vector(31 downto 0);
begin
  u : entity work.t_vhdl_samerangecopy_child generic map (A_G => A_C) port map (o => o);
  process
  begin
    wait for 1 ns;
    report to_hstring(o);
    assert o = x"80000800" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
