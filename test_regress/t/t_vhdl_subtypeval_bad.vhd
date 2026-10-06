-- DESCRIPTION: Verilator: VHDL subtype attribute given to a subprogram as a value
--
-- "resize(din, dout'subtype)" hands RESIZE a subtype where it takes a value
-- (4.2.2, 16.2.2); GHDL refuses it ("no overloaded function", GHDL gna
-- issue2100). It was refused only because nothing could lower it, and once
-- RESIZE's SIZE_RES form could be, it was accepted.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.numeric_std.all;

entity t_vhdl_subtypeval_bad is
end entity;

architecture a of t_vhdl_subtypeval_bad is
  signal din : unsigned(15 downto 0);
  signal dout : unsigned(31 downto 0);
  function f (x : integer) return integer is
  begin
    return x;
  end function;
begin
  dout <= resize(din, dout'subtype);
  process
    variable v : integer;
  begin
    v := f(integer'base);
    wait;
  end process;
end architecture;
