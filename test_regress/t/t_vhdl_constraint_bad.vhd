-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An index constraint applies to an array, or to an access to one; a range
-- constraint to a scalar type; and an index already constrained keeps its
-- constraint, which only "open" may stand in place of.

entity t_vhdl_constraint_bad is
end entity;

architecture a of t_vhdl_constraint_bad is
  type arr2d_t is array (0 to 1) of bit_vector;
  type str_ptr is access string;
  -- Legal
  subtype ok1 is arr2d_t(open)(0 to 3);
  subtype ok2 is str_ptr(1 to 8);
  -- Illegal
  subtype bad1 is integer (0 to 10);
  subtype bad2 is string range 2 to 5;
  subtype bad3 is arr2d_t(0 to 1)(0 to 3);
begin
end architecture;
