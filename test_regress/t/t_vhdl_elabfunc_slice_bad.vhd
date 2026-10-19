-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Elaborating a constant's declaration calls the function its value comes
-- from (14.4.2), though nothing reads the constant, and a slice that function
-- writes outside its prefix stops the model (8.5; GHDL gna issue2960).

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_elabfunc_slice_bad is
end entity;

architecture behav of t_vhdl_elabfunc_slice_bad is
  function f (n : natural) return integer is
    variable v : std_ulogic_vector(3 downto 0) := "0000";
    variable top : integer := n;
  begin
    v(top downto 0) := "000";
    return 42;
  end function;
  constant x : integer := f(9);
begin
  process begin
    wait for 1 ns;
    report "not stopped" severity failure;
    wait;
  end process;
end architecture;
