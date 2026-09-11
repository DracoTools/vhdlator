-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Two packages a use clause makes visible each declare a name, and one of
-- them cannot be overloaded: neither is visible (12.4). STANDARD is one of
-- them, so TIME's unit MIN hides a package's function MIN. Reading the
-- result of a function called with no arguments needs that function to be
-- the only one of its name that can be. References: nvc 1.22.1, GHDL
-- 7.0.0-dev, Questa 2025.2 (nvc takes the function MIN).

package pkg is
  function min(l, r : integer) return integer;
end package;

package body pkg is
  function min(l, r : integer) return integer is
  begin
    if l < r then
      return l;
    end if;
    return r;
  end function;
end package body;

use work.pkg.all;

entity t is
end entity;

architecture a of t is
  function f(b : integer := 0) return string is
  begin
    return "abc";
  end function;
  function f return string is
  begin
    return "def";
  end function;
  function g return string is
  begin
    return "ghi";
  end function;
  signal x, y : integer;
begin
  x <= work.pkg.min(1, 2);
  y <= min(1, 2);  -- Bad
  assert false report g(1 to 2) severity note;
  assert false report f(1 to 2) severity note;  -- Bad
end architecture;
