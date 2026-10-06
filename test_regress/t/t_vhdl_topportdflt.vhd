-- DESCRIPTION: Verilator: VHDL top-level port of an open array type with a default
--
-- "input : in unsigned := "0011"" at the top of the design has nothing
-- associated with it, so it is as long as its default, from the index
-- subtype's own left bound and in its direction (9.3.2). The bounds were
-- parameters nothing gave a value (GHDL gna issue1128).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_topportdflt is
  port (
    input : in unsigned := "0011";
    bits : in bit_vector := x"a5";
    dn : in std_logic_vector := "10");
end entity;

architecture rtl of t_vhdl_topportdflt is
  signal copy : input'subtype;
begin
  process
  begin
    assert input'left = 0 and input'right = 3 severity failure;
    assert input = "0011" severity failure;
    assert bits'length = 8 and bits = x"a5" severity failure;
    assert copy'length = 4 severity failure;
    assert dn'ascending and dn = "10" severity failure;
    report "topportdflt done";
    std.env.finish;
  end process;
end architecture;
