-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- "variable v : t_slv_array(0 to d'length - 1)(7 downto 0) := d" renumbers
-- a parameter from zero: the object's bounds are its own subtype's and the
-- value's elements go in by position (6.4.2.2, 10.6.2.1). Given a slice
-- whose bounds only running settles, the object took the slice's bounds
-- instead, and v(0) read nothing. UVVM's gmii_write normalizes this way,
-- and a transfer written one byte at a time sent zeros.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
package t_vhdl_dynrenumber_pkg is
  type t_slv_array is array (natural range <>) of std_logic_vector;
  subtype t_byte_array is t_slv_array(open)(7 downto 0);
  function first(constant d : in t_slv_array) return std_logic_vector;
  procedure pv(constant d : in t_slv_array; variable o : out std_logic_vector);
end package;
package body t_vhdl_dynrenumber_pkg is
  function first(constant d : in t_slv_array) return std_logic_vector is
    variable v_norm : t_slv_array(0 to d'length - 1)(7 downto 0) := d;
  begin
    return v_norm(0);
  end function;
  procedure pv(constant d : in t_slv_array; variable o : out std_logic_vector) is
    variable v_norm : t_slv_array(0 to d'length - 1)(7 downto 0) := d;
  begin
    o := v_norm(0);
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.t_vhdl_dynrenumber_pkg.all;
entity t_vhdl_dynrenumber is
end entity;
architecture a of t_vhdl_dynrenumber is
  signal data_array : t_byte_array(0 to 9);
begin
  process
    variable o : std_logic_vector(7 downto 0);
    variable va : t_byte_array(0 to 9);
  begin
    for i in data_array'range loop
      data_array(i) <= std_logic_vector(to_unsigned(16 * i + i, 8));
      va(i) := std_logic_vector(to_unsigned(16 * i + i, 8));
    end loop;
    wait for 1 ns;
    for i in 1 to 2 loop
      pv(data_array(i to i + 1), o);
      assert o = std_logic_vector(to_unsigned(17 * i, 8)) and first(data_array(i to i + 1)) = o
        report "signal slice " & integer'image(i) & ": " & to_hstring(o) severity failure;
      pv(va(i to i + 1), o);
      assert o = std_logic_vector(to_unsigned(17 * i, 8)) and first(va(i to i + 1)) = o
        report "variable slice " & integer'image(i) & ": " & to_hstring(o) severity failure;
    end loop;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
