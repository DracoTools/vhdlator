-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An attribute specification gives a value of the attribute's type, to
-- named entities of the class it says.

entity t_vhdl_attrspec_bad is
end entity;

architecture a of t_vhdl_attrspec_bad is
  attribute weight : integer;
  attribute scale : real;
  signal s1 : bit;
  constant c1 : integer := 3;
  -- Legal
  attribute weight of s1 : signal is 4;
  attribute scale of c1 : constant is 2.5;
  -- Illegal
  attribute weight of c1 : signal is 5;
  attribute scale of s1 : signal is 10;
begin
end architecture;
