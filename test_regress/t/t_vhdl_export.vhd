-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- VHDL subprograms called from C. VHDL has no syntax for saying that a
-- subprogram is callable from outside, so it is said in the control file and
-- this source stays portable to other simulators.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_export is
  port (clk : in  std_logic;
        q   : out std_logic_vector(7 downto 0));
end entity;

architecture rtl of t_vhdl_export is

  signal count : unsigned(7 downto 0) := (others => '0');

  function vhdl_square (v : integer) return integer is
  begin
    return v * v;
  end function;

  procedure vhdl_minmax (a : in integer; b : in integer;
                         lo : out integer; hi : out integer) is
  begin
    if a < b then
      lo := a;
      hi := b;
    else
      lo := b;
      hi := a;
    end if;
  end procedure;

begin

  process (clk) is
  begin
    if rising_edge(clk) then
      count <= count + 1;
    end if;
  end process;

  q <= std_logic_vector(count);

end architecture;
