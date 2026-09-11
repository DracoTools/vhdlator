-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Only an array has dimensions, a length or an index range. Before
-- VHDL-2019 a scalar type's range, image and conversions are asked of the
-- type, not of an object of it. An indexed name or a slice has a name or a
-- function call as its prefix: not a qualified expression, nor a type
-- conversion. References: nvc 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

entity t_vhdl_attrprefix_bad is
end entity;

architecture a of t_vhdl_attrprefix_bad is
  type small is range 1 to 10;
  type color is (red, green);
  function f (x : bit_vector) return bit_vector is
  begin
    return x;
  end function;
begin
  process
    variable v : small := 3;
    variable s : small;
    variable n : integer;
    variable b : boolean;
    variable bv : bit_vector(1 to 4) := "0101";
    variable two : bit_vector(1 to 2);
    variable one : bit;
  begin
    -- Legal
    s := small'high;
    n := bv'length;
    n := bv'high(1);
    two := f(bv)(1 to 2);
    one := bv(1);
    -- Illegal
    s := small'high(1);
    s := v'high;
    b := v'ascending;
    n := small'pos(v'base'left);
    n := color'length;
    n := bv(1 to 2)'length + v'length;
    two := bit_vector'("0101")(1 to 2);
    one := bit_vector(bv)(1);
    wait;
  end process;
end architecture;
