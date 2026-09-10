-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An array type is either open in every index or constrained in every
-- index, and a record type declares at least one element.

entity t_vhdl_typedef_bad is
end entity;

architecture a of t_vhdl_typedef_bad is
  type ok1 is array (natural range <>, natural range <>) of bit;
  type ok2 is array (1 to 2, 0 to 3) of bit;
  type bad1 is array (natural range <>, 1 to 2) of bit;
  type bad2 is array (1 to 2, natural range <>) of bit;
  type bad3 is record
  end record;
begin
end architecture;
