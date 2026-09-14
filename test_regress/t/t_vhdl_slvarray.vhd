-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An array of arrays passed as a slice whose ends only running settles.
-- The element says it is eight bits wide; the count does not, so the value
-- travels as a carrier. This is the shape every byte-stream verification
-- component uses.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package p_slvarray is
  type t_slv_array is array (natural range <>) of std_logic_vector;
  procedure fill(data_array : out t_slv_array; data_len : out natural);
  function total(data_array : t_slv_array) return natural;
end package;

package body p_slvarray is
  procedure fill(data_array : out t_slv_array; data_len : out natural) is
    variable k : natural := 0;
  begin
    for i in data_array'range loop
      data_array(i) := std_logic_vector(to_unsigned(k + 1, data_array(i)'length));
      k := k + 1;
    end loop;
    data_len := data_array'length;
  end procedure;

  function total(data_array : t_slv_array) return natural is
    variable acc : natural := 0;
  begin
    for i in data_array'range loop
      acc := acc + to_integer(unsigned(data_array(i)));
    end loop;
    return acc;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.env.all;
use work.p_slvarray.all;

entity t_vhdl_slvarray is
end entity;

architecture tb of t_vhdl_slvarray is
begin
  process is
    variable arr  : t_slv_array(0 to 15)(7 downto 0);
    variable wide : t_slv_array(0 to 7)(15 downto 0);
    variable alen : natural;
    variable n    : natural := 4;
  begin
    -- The slice runs 0 to 3, so fill writes 1, 2, 3, 4
    fill(data_array => arr(0 to n - 1), data_len => alen);
    assert alen = 4 report "bad length " & integer'image(alen) severity failure;
    assert to_integer(unsigned(arr(0))) = 1 report "arr(0)" severity failure;
    assert to_integer(unsigned(arr(3))) = 4 report "arr(3)" severity failure;
    assert total(arr(0 to n - 1)) = 10 report "total" severity failure;

    -- The same type with a sixteen-bit element must not share the eight-bit
    -- carrier: 1 + 2 + 3 + 4 + 5 = 15 over five elements
    n := 5;
    fill(data_array => wide(0 to n - 1), data_len => alen);
    assert alen = 5 report "bad wide length " & integer'image(alen) severity failure;
    assert total(wide(0 to n - 1)) = 15 report "wide total" severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;
end architecture;
