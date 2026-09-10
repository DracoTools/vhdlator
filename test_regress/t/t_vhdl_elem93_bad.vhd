-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Before VHDL-2008 an element of an array or record type has all its
-- bounds; only VHDL-2008 lets an object of the type give them. A bound
-- that names a generic is still a bound.

entity t_vhdl_elem93_bad is
  generic (n : natural := 4);
end entity;

architecture a of t_vhdl_elem93_bad is
  type ok1 is array (1 to 2) of bit_vector(0 to n - 1);
  type ok2 is record
    f : bit_vector(0 to 3);
  end record;
  type bad1 is array (1 to 2) of bit_vector;
  type bad2 is record
    f : bit_vector;
  end record;
begin
end architecture;
