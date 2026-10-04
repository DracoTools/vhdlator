-- DESCRIPTION: Verilator: VHDL generic type given a subtype whose length
-- names a generic
--
-- A package with a generic type and an array type of it is instantiated
-- in an entity's declarative part with "data_type =>
-- std_logic_vector(WIDTH - 1 downto 0)", WIDTH a generic of the entity. A
-- variable of the array type declared with its index range has all its
-- bounds: the element's length names a generic, but it is a length (GHDL
-- gna issue2580).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_gentypegenw_pkg is
  generic (type data_type);
  type array_data_type is array (natural range <>) of data_type;
end package;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_gentypegenw is
  generic (WIDTH : positive := 8; LENGTH : positive := 4);
  package test_pkg_def is new work.t_vhdl_gentypegenw_pkg
    generic map (data_type => std_logic_vector(WIDTH - 1 downto 0));
  use test_pkg_def.all;
end entity;

architecture behavior of t_vhdl_gentypegenw is
begin
  process is
    variable increments : array_data_type(0 to LENGTH - 1);
    variable total : natural := 0;
  begin
    for i in 0 to LENGTH - 1 loop
      increments(i) := std_logic_vector(to_unsigned(i * 3, WIDTH));
    end loop;
    for i in increments'range loop
      total := total + to_integer(unsigned(increments(i)));
    end loop;
    assert increments(0)'length = WIDTH and increments'length = LENGTH severity failure;
    assert increments(3) = x"09" and total = 18 severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
