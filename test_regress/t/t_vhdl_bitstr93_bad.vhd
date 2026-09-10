-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Before VHDL-2008 a bit string literal holds only the digits of its base.
-- VHDL-2008 passes any other character through, so B"XX" is two 'X'
-- elements there, but in VHDL-93 it is an error in the literal itself.

entity t_vhdl_bitstr93_bad is
end entity;

architecture a of t_vhdl_bitstr93_bad is
  constant ok : bit_vector := X"0f_A7";
  constant bad1 : bit_vector := B"0012";
  constant bad2 : bit_vector := X"2AG";
  constant bad3 : bit_vector := O"78";
begin
end architecture;
