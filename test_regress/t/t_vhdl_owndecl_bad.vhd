-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A declaration's scope begins where the declaration does, so within it the
-- name denotes nothing yet -- not the outer declaration it is about to hide.
-- An alias of a subprogram is overloadable and may repeat the designator.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_owndecl_bad is
end entity;

architecture a of t_vhdl_owndecl_bad is
  constant c1 : integer := 1;
  subtype t1 is integer;
  signal s1 : bit_vector(1 to 6);
  alias a1 is c1;
  -- Legal
  alias "??" is "??" [std_ulogic return boolean];
begin
  b1 : block is
    -- Illegal
    signal s1 : bit_vector(1 to s1'length);
    constant c1 : integer := c1 + 1;
    subtype t1 is t1 range 0 to 3;
    alias a1 is a1;
  begin
  end block;
end architecture;
