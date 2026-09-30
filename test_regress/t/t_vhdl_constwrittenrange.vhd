-- DESCRIPTION: Verilator: VHDL constant with a range that does not fold
--
-- This file is part of Verilator.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
--
-- A constant declared with a range whose bounds are not numbers while
-- compiling is indexed by that range, not by its value's: VUnit's
-- to_nibble_string declares "constant value_i : unsigned(value'length
-- downto 1) := value", and walks value_i'range. Called for a signal a
-- generic sizes, it walked the value's own 7 downto 0.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package t_vhdl_constwrittenrange_pkg is
  function to_nib(constant value : unsigned) return string;
  function to_nib(constant value : std_logic_vector) return string;
end package;

package body t_vhdl_constwrittenrange_pkg is
  function to_nib(constant value : unsigned) return string is
    constant value_i : unsigned(value'length downto 1) := value;
    variable ret_val : string(1 to (value'length + (value'length - 1) / 4));
    variable index   : natural := 1;
  begin
    assert value_i'left = value'length and value_i'right = 1
      report "value_i'range " & integer'image(value_i'left) & " to "
      & integer'image(value_i'right) severity failure;
    for i in value_i'range loop
      if (i mod 4 = 0) and (i /= value_i'left) then
        ret_val(index) := '_';
        index := index + 1;
      end if;
      ret_val(index) := std_logic'image(value_i(i))(2);
      index := index + 1;
    end loop;
    return ret_val;
  end;

  function to_nib(constant value : std_logic_vector) return string is
  begin
    return to_nib(unsigned(value));
  end;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_constwrittenrange_pkg.all;

entity t_vhdl_constwrittenrange is
  generic (dl : natural := 8);
end entity;

architecture a of t_vhdl_constwrittenrange is
  signal wide  : std_logic_vector(dl - 1 downto 0) := x"a5";
  signal asc   : std_logic_vector(0 to dl + 3) := x"c3a";
  constant fixed : std_logic_vector(7 downto 0) := x"3c";
begin
  process
  begin
    wait for 1 ns;
    report "fixed " & to_nib(fixed) & " wide " & to_nib(wide) & " asc " & to_nib(asc);
    assert to_nib(fixed) = "0011_1100" report "fixed" severity failure;
    assert to_nib(wide) = "1010_0101" report "wide" severity failure;
    assert to_nib(asc) = "1100_0011_1010" report "asc" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
