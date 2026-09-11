-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An object alias names an object, which the value of a function call, a
-- conversion or an attribute is not; and an alias's signature names one of
-- the declarations of what it aliases.

entity t_vhdl_aliasobj_bad is
end entity;

architecture a of t_vhdl_aliasobj_bad is
  signal s : bit_vector(7 downto 0);
  signal b : bit;
  function f (v : bit_vector) return bit_vector is
  begin
    return v;
  end function;
  procedure p is
  begin
  end procedure;
  -- Legal
  alias ok1 : bit_vector(7 downto 0) is s;
  alias ok2 is p [];
  -- Illegal
  alias bad1 : bit_vector(7 downto 0) is f(s);
  alias bad2 : string is bit'image(b);
  alias bad3 is p [return integer];
begin
end architecture;
