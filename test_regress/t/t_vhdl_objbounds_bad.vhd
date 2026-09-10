-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Only a constant takes its bounds from its value. A signal or a variable
-- is given them where it is declared, since what it holds later can be of
-- any length its subtype allows; a bound known only when a subprogram is
-- called is still a bound. The elements of a record have distinct names.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_objbounds_bad is
end entity;

architecture a of t_vhdl_objbounds_bad is
  type rec is record
    f : bit_vector;
    g : bit;
  end record;
  type dup is record
    x : bit;
    x : integer;
  end record;
  subtype word is std_logic_vector(7 downto 0);
  subtype open_t is std_logic_vector;
  -- Legal
  constant c : bit_vector := "0101";
  signal w : word;
  signal r : rec(f(1 to 4));
  -- Illegal
  signal s1 : bit_vector;
  signal s2 : open_t;
  signal s3 : rec;
  signal s4 : string;
  function f (x : bit_vector) return bit_vector is
    variable v1 : bit_vector(x'range);
    variable v2 : bit_vector(1 to x'length);
    variable v3 : bit_vector := x;
  begin
    return v1 & v2 & v3;
  end function;
begin
end architecture;
