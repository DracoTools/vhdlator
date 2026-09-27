-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- An alias that renumbers a value only running measures: UVVM's
-- normalize_and_check and matching_widths write "alias a_value :
-- std_logic_vector(value'length - 1 downto 0) is value" over a parameter
-- (uvvm_util/src/bfm_common_pkg.vhd, methods_pkg.vhd). Its attributes,
-- slices and elements count from the alias's bounds (6.6.2), but they were
-- the value's own -- "0 to 39" for a literal given to an open parameter --
-- so a_value(31 downto 0) of x"0000000510" came back A0000000. And a
-- search for 'Z' through a parameter, idx_leftmost_p1_in_p2('Z', ...),
-- found every zero of such a value, since in two states a 'Z' held in an
-- object reads as '0': a copy is now made for the value compared, so the
-- comparison is with the literal. Together these made normalize_and_check
-- refuse every wider address UVVM's AXI testbench gives a BFM.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_aliascarried is
end entity;

architecture a of t_vhdl_aliascarried is
  function leftmost(target : std_logic; vector : std_logic_vector) return integer is
    alias a_vector : std_logic_vector(vector'length - 1 downto 0) is vector;
  begin
    for i in a_vector'left downto a_vector'right loop
      if a_vector(i) = target then
        return i;
      end if;
    end loop;
    return -1;
  end function;

  function describe(constant value : in std_logic_vector) return string is
    alias a_value : std_logic_vector(value'length - 1 downto 0) is value;
  begin
    return integer'image(a_value'left) & " " & integer'image(a_value'right) & " "
      & std_logic'image(a_value(4)) & " " & to_hstring(a_value(31 downto 0)) & " "
      & integer'image(leftmost('Z', a_value)) & " " & integer'image(leftmost('1', a_value));
  end function;

  -- The value handed on only running measures
  function through(constant v : in unsigned; n : natural) return string is
  begin
    return describe(std_logic_vector(v(0 to n)));
  end function;
begin
  process
    variable n : natural := 39;
  begin
    assert through(x"0000000510", n) = "39 0 '1' 00000510 -1 10"
      report through(x"0000000510", n) severity failure;
    assert describe(std_logic_vector'(x"0000000510")) = "39 0 '1' 00000510 -1 10"
      report describe(std_logic_vector'(x"0000000510")) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
