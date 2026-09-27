-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A generic type stands for a type nothing is known of until the package is
-- instantiated, so a value of it has only the operations the generic list
-- gives it (6.5.3): no subprogram written for some other type takes it, and
-- it has no length. Both were accepted, since the type is kept apart from
-- every other as a type in error, and an argument in error said nothing.
-- nvc 1.22.1, GHDL 7.0.0-dev and Questa 25.1std refuse both.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package t_vhdl_gentypeops_bad is
  generic (type t_element;
           function to_string_element(e : t_element) return string);
  function f(a : t_element) return string;
  function g(a : t_element) return natural;
  function h(a : t_element) return string;
end package;

package body t_vhdl_gentypeops_bad is
  function f(a : t_element) return string is
  begin
    return to_hstring(a);  -- BAD: no TO_HSTRING takes a T_ELEMENT
  end function;

  function g(a : t_element) return natural is
  begin
    return a'length;  -- BAD: a T_ELEMENT is not known to be an array
  end function;

  -- What the generic list gives it is fine
  function h(a : t_element) return string is
  begin
    if a = a then
      return to_string_element(a);
    end if;
    return "";
  end function;
end package body;
